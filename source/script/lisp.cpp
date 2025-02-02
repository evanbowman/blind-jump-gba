#include "lisp.hpp"
#include "bulkAllocator.hpp"
#include "bytecode.hpp"
#include "listBuilder.hpp"
#include "localization.hpp"
#include "memory/buffer.hpp"
#include "memory/pool.hpp"
#include <complex>
#ifdef __GBA__
#define HEAP_DATA __attribute__((section(".ewram")))
#else
#include <iostream>
#define HEAP_DATA
#endif


namespace lisp {


static int run_gc();


static const u32 string_intern_table_size = 1999;


#define VALUE_POOL_SIZE 9000


union ValueMemory {
    Value value_;
    HeapNode heap_node_;
    Nil nil_;
    Integer integer_;
    Cons cons_;
    Function function_;
    Error error_;
    Symbol symbol_;
    UserData user_data_;
    DataBuffer data_buffer_;
    String string_;
    Character character_;
    __Reserved __reserved_;
};


#ifdef __GBA__
static_assert(sizeof(ValueMemory) == 8);
#endif


static HEAP_DATA ValueMemory value_pool_data[VALUE_POOL_SIZE];
static Value* value_pool = nullptr;


void value_pool_init()
{
    for (int i = 0; i < VALUE_POOL_SIZE; ++i) {
        auto v = (Value*)(value_pool_data + i);

        v->hdr_.alive_ = false;
        v->hdr_.mark_bit_ = false;
        v->hdr_.type_ = Value::Type::heap_node;

        v->heap_node().next_ = value_pool;
        value_pool = v;
    }
}


Value* value_pool_alloc()
{
    if (value_pool) {
        auto ret = value_pool;
        value_pool = ret->heap_node().next_;
        return (Value*)ret;
    }
    return nullptr;
}


void value_pool_free(Value* value)
{
    value->hdr_.type_ = Value::Type::heap_node;
    value->hdr_.alive_ = false;
    value->hdr_.mark_bit_ = false;

    value->heap_node().next_ = value_pool;
    value_pool = value;
}


struct Context {
    using OperandStack = Buffer<Value*, 497>;

    using Interns = char[string_intern_table_size];

    Context(Platform& pfrm)
        : operand_stack_(allocate_dynamic<OperandStack>(pfrm)),
          interns_(allocate_dynamic<Interns>(pfrm)), pfrm_(pfrm)
    {
        if (not operand_stack_ or not interns_) {
            pfrm_.fatal("pointer compression test failed");
        }
    }

    DynamicMemory<OperandStack> operand_stack_;
    DynamicMemory<Interns> interns_;

    u16 arguments_break_loc_;
    u8 current_fn_argc_ = 0;
    Value* this_ = nullptr;


    Value* nil_ = nullptr;
    Value* oom_ = nullptr;
    Value* string_buffer_ = nullptr;
    Value* globals_tree_ = nullptr;

    Value* lexical_bindings_ = nullptr;
    Value* macros_ = nullptr;

    const IntegralConstant* constants_ = nullptr;
    u16 constants_count_ = 0;

    int string_intern_pos_ = 0;
    int eval_depth_ = 0;
    int interp_entry_count_ = 0;

    Platform& pfrm_;
};


static std::optional<Context> bound_context;


// Globals tree node:
// ((key . value) . (left-child . right-child))
//
// i.e.: Each global variable binding uses three cons cells.


static void globals_tree_insert(Value* key, Value* value)
{
    auto& ctx = *bound_context;

    Protected new_kvp(make_cons(key, value));

    if (ctx.globals_tree_ == get_nil()) {
        // The empty set of left/right children
        push_op(make_cons(get_nil(), get_nil()));

        auto new_tree = make_cons(new_kvp, get_op0());
        pop_op();

        ctx.globals_tree_ = new_tree;

    } else {
        // Ok, if the tree exists, now we need to scan the tree, looking for the
        // key. If it exists, replace the existing value with our new
        // value. Otherwise, insert key at the terminal point.

        Protected current(ctx.globals_tree_);
        Protected prev(ctx.globals_tree_);
        bool insert_left = true;

        while (current not_eq get_nil()) {

            auto current_key = current->cons().car()->cons().car();

            if (current_key->symbol().name_ == key->symbol().name_) {
                // The key alreay exists, overwrite the previous value.
                current->cons().car()->cons().set_cdr(value);
                return;

            } else {
                prev = (Value*)current;

                if (current_key->symbol().name_ < key->symbol().name_) {
                    // Continue loop through left subtree
                    insert_left = true;
                    current = current->cons().cdr()->cons().car();
                } else {
                    // Continue loop through right subtree
                    insert_left = false;
                    current = current->cons().cdr()->cons().cdr();
                }
            }
        }

        if (insert_left) {
            push_op(make_cons(get_nil(), get_nil()));

            auto new_tree = make_cons(new_kvp, get_op0());
            pop_op();

            prev->cons().cdr()->cons().set_car(new_tree);

        } else {
            push_op(make_cons(get_nil(), get_nil()));

            auto new_tree = make_cons(new_kvp, get_op0());
            pop_op();

            prev->cons().cdr()->cons().set_cdr(new_tree);
        }
    }
}


using GlobalsTreeVisitor = ::Function<24, void(Value&, Value&)>;


static Value* left_subtree(Value* tree)
{
    return tree->cons().cdr()->cons().car();
}


static Value* right_subtree(Value* tree)
{
    return tree->cons().cdr()->cons().cdr();
}


static void set_right_subtree(Value* tree, Value* value)
{
    tree->cons().cdr()->cons().set_cdr(value);
}


// Invokes callback with (key . value) for each global var definition.
// In place traversal, using Morris algorithm.
static void globals_tree_traverse(Value* root, GlobalsTreeVisitor callback)
{
    if (root == get_nil()) {
        return;
    }

    auto current = root;
    auto prev = get_nil();

    while (current not_eq get_nil()) {

        if (left_subtree(current) == get_nil()) {
            callback(*current->cons().car(), *current);
            current = right_subtree(current);
        } else {
            prev = left_subtree(current);

            while (right_subtree(prev) not_eq get_nil() and
                   right_subtree(prev) not_eq current) {
                prev = right_subtree(prev);
            }

            if (right_subtree(prev) == get_nil()) {
                set_right_subtree(prev, current);
                current = left_subtree(current);
            } else {
                set_right_subtree(prev, get_nil());
                callback(*current->cons().car(), *current);
                current = right_subtree(current);
            }
        }
    }
}


static void globals_tree_erase(Value* key)
{
    auto& ctx = *bound_context;

    if (ctx.globals_tree_ == get_nil()) {
        return;
    }

    auto current = ctx.globals_tree_;
    auto prev = current;
    bool erase_left = true;

    while (current not_eq get_nil()) {

        auto current_key = current->cons().car()->cons().car();

        if (current_key->symbol().name_ == key->symbol().name_) {

            Protected erased(current);

            if (current == prev) {
                ctx.globals_tree_ = get_nil();
            } else {
                if (erase_left) {
                    prev->cons().cdr()->cons().set_car(get_nil());
                } else {
                    prev->cons().cdr()->cons().set_cdr(get_nil());
                }
            }

            auto reattach_child = [](Value& kvp, Value&) {
                globals_tree_insert(kvp.cons().car(), kvp.cons().cdr());
            };

            auto left_child = erased->cons().cdr()->cons().car();
            if (left_child not_eq get_nil()) {
                globals_tree_traverse(left_child, reattach_child);
            }

            auto right_child = erased->cons().cdr()->cons().cdr();
            if (right_child not_eq get_nil()) {
                globals_tree_traverse(right_child, reattach_child);
            }

            return;
        }

        prev = current;

        if (current_key->symbol().name_ < key->symbol().name_) {
            erase_left = true;
            current = current->cons().cdr()->cons().car();
        } else {
            erase_left = false;
            current = current->cons().cdr()->cons().cdr();
        }
    }
}


static Value* globals_tree_find(Value* key)
{
    auto& ctx = *bound_context;

    if (ctx.globals_tree_ == get_nil()) {
        return get_nil();
    }

    auto current = ctx.globals_tree_;

    while (current not_eq get_nil()) {

        auto current_key = current->cons().car()->cons().car();

        if (current_key->symbol().name_ == key->symbol().name_) {
            return current->cons().car()->cons().cdr();
        }

        if (current_key->symbol().name_ < key->symbol().name_) {
            current = current->cons().cdr()->cons().car();
        } else {
            current = current->cons().cdr()->cons().cdr();
        }
    }

    StringBuffer<31> hint("[var: ");
    hint += key->symbol().name_;
    hint += "]";

    return make_error(Error::Code::undefined_variable_access,
                      make_string(bound_context->pfrm_, hint.c_str()));
}


static bool is_list(Value* val)
{
    while (val not_eq get_nil()) {
        if (val->type() not_eq Value::Type::cons) {
            return false;
        }
        val = val->cons().cdr();
    }
    return true;
}


void set_constants(const IntegralConstant* constants, u16 count)
{
    if (not bound_context) {
        return;
    }

    bound_context->constants_ = constants;
    bound_context->constants_count_ = count;
}


u16 symbol_offset(const char* symbol)
{
    return symbol - *bound_context->interns_;
}


const char* symbol_from_offset(u16 offset)
{
    return *bound_context->interns_ + offset;
}


Value* get_nil()
{
    return bound_context->nil_;
}


void get_interns(::Function<24, void(const char*)> callback)
{
    auto& ctx = bound_context;

    const char* search = *ctx->interns_;
    for (int i = 0; i < ctx->string_intern_pos_;) {
        callback(search + i);
        while (search[i] not_eq '\0') {
            ++i;
        }
        ++i;
    }

    for (u16 i = 0; i < bound_context->constants_count_; ++i) {
        callback((const char*)bound_context->constants_[i].name_);
    }
}


void get_env(::Function<24, void(const char*)> callback)
{
    auto& ctx = bound_context;

    globals_tree_traverse(ctx->globals_tree_, [&callback](Value& val, Value&) {
        callback((const char*)val.cons().car()->symbol().name_);
    });

    for (u16 i = 0; i < bound_context->constants_count_; ++i) {
        callback((const char*)bound_context->constants_[i].name_);
    }
}


Value* get_arg(u16 n)
{
    auto br = bound_context->arguments_break_loc_;
    auto argc = bound_context->current_fn_argc_;
    if (br >= ((argc - 1) - n)) {
        return (*bound_context->operand_stack_)[br - ((argc - 1) - n)];
    } else {
        return get_nil();
    }
}


const char* intern(const char* string)
{
    const auto len = str_len(string);

    if (len + 1 >
        string_intern_table_size - bound_context->string_intern_pos_) {

        bound_context->pfrm_.fatal("string intern table full");
    }

    auto& ctx = bound_context;

    const char* search = *ctx->interns_;
    for (int i = 0; i < ctx->string_intern_pos_;) {
        if (str_cmp(search + i, string) == 0) {
            return search + i;
        } else {
            while (search[i] not_eq '\0') {
                ++i;
            }
            ++i;
        }
    }

    auto result = *ctx->interns_ + ctx->string_intern_pos_;

    for (u32 i = 0; i < len; ++i) {
        (*ctx->interns_)[ctx->string_intern_pos_++] = string[i];
    }
    (*ctx->interns_)[ctx->string_intern_pos_++] = '\0';

    return result;
}


CompressedPtr compr(Value* val)
{
    CompressedPtr result;

#ifdef USE_COMPRESSED_PTRS
    static_assert(sizeof(ValueMemory) % 2 == 0);
    result.offset_ = ((u8*)val - (u8*)value_pool_data) / sizeof(ValueMemory);
#else
    result.ptr_ = val;
#endif

    return result;
}


Value* dcompr(CompressedPtr ptr)
{
#ifdef USE_COMPRESSED_PTRS
    auto ret =
        (Value*)(((ptr.offset_ * sizeof(ValueMemory)) + (u8*)value_pool_data));
    return ret;
#else
    return (Value*)ptr.ptr_;
#endif // USE_COMPRESSED_PTRS
}


int length(Value* lat)
{
    int len = 0;
    while (true) {
        ++len;
        lat = lat->cons().cdr();
        if (lat->type() not_eq Value::Type::cons) {
            if (lat not_eq get_nil()) {
                return 0; // not a well-formed list
            }
            break;
        }
    }
    return len;
}


Value* Function::Bytecode::bytecode_offset() const
{
    return dcompr(bytecode_)->cons().car();
}


Value* Function::Bytecode::databuffer() const
{
    return dcompr(bytecode_)->cons().cdr();
}


static Value* alloc_value()
{
    auto init_val = [](Value* val) {
        val->hdr_.mark_bit_ = false;
        val->hdr_.alive_ = true;
        return val;
    };

    if (auto val = value_pool_alloc()) {
        return init_val(val);
    }

    run_gc();

    // Hopefully, we've freed up enough memory...
    if (auto val = value_pool_alloc()) {
        return init_val(val);
    }

    return nullptr;
}


Value* make_function(Function::CPP_Impl impl)
{
    if (auto val = alloc_value()) {
        val->hdr_.type_ = Value::Type::function;
        val->function().cpp_impl_ = impl;
        val->hdr_.mode_bits_ = Function::ModeBits::cpp_function;
        return val;
    }
    return bound_context->oom_;
}


static Value* make_lisp_function(Value* impl)
{
    if (auto val = alloc_value()) {
        val->hdr_.type_ = Value::Type::function;
        val->function().lisp_impl_.code_ = compr(impl);
        val->function().lisp_impl_.lexical_bindings_ =
            compr(bound_context->lexical_bindings_);

        val->hdr_.mode_bits_ = Function::ModeBits::lisp_function;
        return val;
    }
    return bound_context->oom_;
}


Value* make_bytecode_function(Value* bytecode)
{
    if (auto val = alloc_value()) {
        val->hdr_.type_ = Value::Type::function;
        val->function().bytecode_impl_.lexical_bindings_ =
            compr(bound_context->lexical_bindings_);

        val->function().bytecode_impl_.bytecode_ = compr(bytecode);
        val->hdr_.mode_bits_ = Function::ModeBits::lisp_bytecode_function;
        return val;
    }
    return bound_context->oom_;
}


Value* make_cons(Value* car, Value* cdr)
{
    if (auto val = alloc_value()) {
        val->hdr_.type_ = Value::Type::cons;
        val->cons().set_car(car);
        val->cons().set_cdr(cdr);
        return val;
    }
    return bound_context->oom_;
}


Value* make_integer(s32 value)
{
    if (auto val = alloc_value()) {
        val->hdr_.type_ = Value::Type::integer;
        val->integer().value_ = value;
        return val;
    }
    return bound_context->oom_;
}


Value* make_list(u32 length)
{
    if (length == 0) {
        return get_nil();
    }
    auto head = make_cons(get_nil(), get_nil());
    while (--length) {
        push_op(head); // To keep head from being collected, in case make_cons()
                       // triggers the gc.
        auto cell = make_cons(get_nil(), head);
        pop_op(); // head

        head = cell;
    }
    return head;
}


Value* make_error(Error::Code error_code, Value* context)
{
    if (auto val = alloc_value()) {
        val->hdr_.type_ = Value::Type::error;
        val->error().code_ = error_code;
        val->error().context_ = compr(context);
        return val;
    }
    return bound_context->oom_;
}


Value* make_symbol(const char* name, Symbol::ModeBits mode)
{
    if (auto val = alloc_value()) {
        val->hdr_.type_ = Value::Type::symbol;
        val->symbol().name_ = [mode, name] {
            switch (mode) {
            case Symbol::ModeBits::requires_intern:
                break;

            case Symbol::ModeBits::stable_pointer:
                return name;
            }
            return intern(name);
        }();
        return val;
    }
    return bound_context->oom_;
}


static Value* intern_to_symbol(const char* already_interned_str)
{
    if (auto val = alloc_value()) {
        val->hdr_.type_ = Value::Type::symbol;
        val->symbol().name_ = already_interned_str;
        return val;
    }
    return bound_context->oom_;
}


Value* make_userdata(void* obj)
{
    if (auto val = alloc_value()) {
        val->hdr_.type_ = Value::Type::user_data;
        val->user_data().obj_ = obj;
        return val;
    }
    return bound_context->oom_;
}


Value* make_databuffer(Platform& pfrm)
{
    if (not pfrm.scratch_buffers_remaining()) {
        // Collect any data buffers that may be lying around.
        run_gc();
    }

    if (auto val = alloc_value()) {
        val->hdr_.type_ = Value::Type::data_buffer;
        new ((ScratchBufferPtr*)val->data_buffer().sbr_mem_)
            ScratchBufferPtr(pfrm.make_scratch_buffer());
        return val;
    }
    return bound_context->oom_;
}


void live_values(::Function<24, void(Value&)> callback);


Value* make_string(Platform& pfrm, const char* string)
{
    auto len = str_len(string);

    Value* existing_buffer = nullptr;
    decltype(len) free = 0;

    if (bound_context->string_buffer_ not_eq L_NIL) {
        auto buffer = bound_context->string_buffer_;
        free = 0;
        for (int i = SCRATCH_BUFFER_SIZE - 1; i > 0; --i) {
            if (buffer->data_buffer().value()->data_[i] == '\0') {
                ++free;
            } else {
                break;
            }
        }
        if (free > len + 1) { // +1 for null term, > for other null term
            existing_buffer = buffer;
        } else {
            bound_context->string_buffer_ = L_NIL;
        }
    }


    if (existing_buffer) {
        const auto offset = (SCRATCH_BUFFER_SIZE - free) + 1;

        auto write_ptr = existing_buffer->data_buffer().value()->data_ + offset;

        while (*string) {
            *write_ptr++ = *string++;
        }

        if (auto val = alloc_value()) {
            val->hdr_.type_ = Value::Type::string;
            val->string().data_buffer_ = compr(existing_buffer);
            val->string().offset_ = offset;
            return val;
        } else {
            return bound_context->oom_;
        }
    } else {
        auto buffer = make_databuffer(pfrm);

        if (buffer == bound_context->oom_) {
            return bound_context->oom_;
        }

        Protected p(buffer);
        bound_context->string_buffer_ = buffer;

        for (int i = 0; i < SCRATCH_BUFFER_SIZE; ++i) {
            buffer->data_buffer().value()->data_[i] = '\0';
        }
        auto write_ptr = buffer->data_buffer().value()->data_;

        while (*string) {
            *write_ptr++ = *string++;
        }

        if (auto val = alloc_value()) {
            val->hdr_.type_ = Value::Type::string;
            val->string().data_buffer_ = compr(buffer);
            val->string().offset_ = 0;
            return val;
        } else {
            return bound_context->oom_;
        }
    }
}


void set_list(Value* list, u32 position, Value* value)
{
    while (position--) {
        if (list->type() not_eq Value::Type::cons) {
            // TODO: raise error
            return;
        }
        list = list->cons().cdr();
    }

    if (list->type() not_eq Value::Type::cons) {
        // TODO: raise error
        return;
    }

    list->cons().set_car(value);
}


Value* get_list(Value* list, u32 position)
{
    while (position--) {
        if (list->type() not_eq Value::Type::cons) {
            // TODO: raise error
            return get_nil();
        }
        list = list->cons().cdr();
    }

    if (list->type() not_eq Value::Type::cons) {
        // TODO: raise error
        return get_nil();
    }

    return list->cons().car();
}


void pop_op()
{
    bound_context->operand_stack_->pop_back();
}


void push_op(Value* operand)
{
    bound_context->operand_stack_->push_back(operand);
}


void insert_op(u32 offset, Value* operand)
{
    auto& stack = bound_context->operand_stack_;
    auto pos = stack->end() - offset;
    stack->insert(pos, operand);
}


Value* get_op0()
{
    auto& stack = bound_context->operand_stack_;
    return stack->back();
}


Value* get_op1()
{
    auto& stack = bound_context->operand_stack_;
    return *(stack->end() - 2);
}


Value* get_op(u32 offset)
{
    auto& stack = bound_context->operand_stack_;
    if (offset >= stack->size()) {
        return get_nil(); // TODO: raise error
    }

    return (*stack)[(stack.obj_->size() - 1) - offset];
}


void lexical_frame_push()
{
    bound_context->lexical_bindings_ =
        make_cons(get_nil(), bound_context->lexical_bindings_);
}


void lexical_frame_pop()
{
    bound_context->lexical_bindings_ =
        bound_context->lexical_bindings_->cons().cdr();
}


void lexical_frame_store(Value* kvp)
{
    bound_context->lexical_bindings_->cons().set_car(
        make_cons(kvp, bound_context->lexical_bindings_->cons().car()));
}


void vm_execute(Platform& pfrm, Value* code, int start_offset);


// The function arguments should be sitting at the top of the operand stack
// prior to calling funcall. The arguments will be consumed, and replaced with
// the result of the function call.
void funcall(Value* obj, u8 argc)
{
    auto pop_args = [&argc] {
        for (int i = 0; i < argc; ++i) {
            bound_context->operand_stack_->pop_back();
        }
    };

    // NOTE: The callee must be somewhere on the operand stack, so it's safe
    // to store this unprotected var here.
    Value* prev_this = get_this();
    Value* prev_bindings = bound_context->lexical_bindings_;

    auto& ctx = *bound_context;
    auto prev_arguments_break_loc = ctx.arguments_break_loc_;
    auto prev_argc = ctx.current_fn_argc_;

    switch (obj->type()) {
    case Value::Type::function: {
        if (bound_context->operand_stack_->size() < argc) {
            pop_args();
            push_op(make_error(Error::Code::invalid_argc, obj));
            break;
        }

        switch (obj->hdr_.mode_bits_) {
        case Function::ModeBits::cpp_function: {
            auto result = obj->function().cpp_impl_(argc);
            pop_args();
            push_op(result);
            break;
        }

        case Function::ModeBits::lisp_function: {
            auto& ctx = *bound_context;
            ctx.lexical_bindings_ =
                dcompr(obj->function().lisp_impl_.lexical_bindings_);
            const auto break_loc = ctx.operand_stack_->size() - 1;
            auto expression_list = dcompr(obj->function().lisp_impl_.code_);
            auto result = get_nil();
            push_op(result);
            while (expression_list not_eq get_nil()) {
                if (expression_list->type() not_eq Value::Type::cons) {
                    break;
                }
                pop_op(); // result
                ctx.arguments_break_loc_ = break_loc;
                ctx.current_fn_argc_ = argc;
                ctx.this_ = obj;
                eval(expression_list->cons().car()); // new result
                expression_list = expression_list->cons().cdr();
            }
            result = get_op0();
            pop_op(); // result
            pop_args();
            push_op(result);
            break;
        }

        case Function::ModeBits::lisp_bytecode_function: {
            auto& ctx = *bound_context;
            const auto break_loc = ctx.operand_stack_->size() - 1;
            ctx.arguments_break_loc_ = break_loc;
            ctx.current_fn_argc_ = argc;
            ctx.this_ = obj;

            ctx.lexical_bindings_ =
                dcompr(obj->function().lisp_impl_.lexical_bindings_);

            vm_execute(bound_context->pfrm_,
                       obj->function().bytecode_impl_.databuffer(),
                       obj->function()
                           .bytecode_impl_.bytecode_offset()
                           ->integer()
                           .value_);

            auto result = get_op0();
            pop_op();
            pop_args();
            push_op(result);
            break;
        }
        }
        break;
    }

    default:
        push_op(make_error(Error::Code::value_not_callable, L_NIL));
        break;
    }

    bound_context->this_ = prev_this;
    bound_context->lexical_bindings_ = prev_bindings;
    ctx.arguments_break_loc_ = prev_arguments_break_loc;
    ctx.current_fn_argc_ = prev_argc;
}


u8 get_argc()
{
    return bound_context->current_fn_argc_;
}


Value* get_this()
{
    return bound_context->this_;
}


Value* get_var_stable(const char* intern_str)
{
    return get_var(make_symbol(intern_str, Symbol::ModeBits::stable_pointer));
}


Value* get_var(Value* symbol)
{
    if (symbol->symbol().name_[0] == '$') {
        if (symbol->symbol().name_[1] == 'V') {
            // Special case: use '$V' to access arguments as a list.
            ListBuilder lat;
            for (int i = bound_context->current_fn_argc_ - 1; i > -1; --i) {
                lat.push_front(get_arg(i));
            }
            return lat.result();
        } else {
            s32 argn = 0;
            for (u32 i = 1; symbol->symbol().name_[i] not_eq '\0'; ++i) {
                argn = argn * 10 + (symbol->symbol().name_[i] - '0');
            }

            return get_arg(argn);
        }
    }

    if (bound_context->lexical_bindings_ not_eq get_nil()) {
        auto stack = bound_context->lexical_bindings_;

        while (stack not_eq get_nil()) {

            auto bindings = stack->cons().car();
            while (bindings not_eq get_nil()) {
                auto kvp = bindings->cons().car();
                if (kvp->cons().car()->symbol().name_ ==
                    symbol->symbol().name_) {
                    return kvp->cons().cdr();
                }

                bindings = bindings->cons().cdr();
            }

            stack = stack->cons().cdr();
        }
    }

    auto found = globals_tree_find(symbol);

    if (found->type() not_eq Value::Type::error) {
        return found;
    } else {
        for (u16 i = 0; i < bound_context->constants_count_; ++i) {
            const auto& k = bound_context->constants_[i];
            if (str_cmp(k.name_, symbol->symbol().name_) == 0) {
                return lisp::make_integer(k.value_);
            }
        }
        return found;
    }
}


Value* set_var(Value* symbol, Value* val)
{
    if (bound_context->lexical_bindings_ not_eq get_nil()) {
        auto stack = bound_context->lexical_bindings_;

        while (stack not_eq get_nil()) {

            auto bindings = stack->cons().car();
            while (bindings not_eq get_nil()) {
                auto kvp = bindings->cons().car();
                if (kvp->cons().car()->symbol().name_ ==
                    symbol->symbol().name_) {

                    kvp->cons().set_cdr(val);
                    return get_nil();
                }

                bindings = bindings->cons().cdr();
            }

            stack = stack->cons().cdr();
        }
    }

    globals_tree_insert(symbol, val);
    return get_nil();
}


bool is_boolean_true(Value* val)
{
    switch (val->type()) {
    case Value::Type::integer:
        return val->integer().value_ not_eq 0;

    default:
        break;
    }

    return val not_eq get_nil();
}


static const long hextable[] = {
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 0,  1,  2,  3,  4,  5,  6,  7,  8,
    9,  -1, -1, -1, -1, -1, -1, -1, 10, 11, 12, 13, 14, 15, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, 10, 11, 12, 13, 14, 15, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1};


long hexdec(unsigned const char* hex)
{
    long ret = 0;
    while (*hex && ret >= 0) {
        ret = (ret << 4) | hextable[*hex++];
    }
    return ret;
}


bool is_executing()
{
    if (bound_context) {
        return bound_context->interp_entry_count_;
    }

    return false;
}


Value* dostring(const char* code, ::Function<16, void(Value&)> on_error)
{
    if (code == nullptr) {
        on_error(*L_NIL);
    }

    ++bound_context->interp_entry_count_;

    int i = 0;

    Protected result(get_nil());

    while (true) {
        i += read(code + i);
        auto reader_result = get_op0();
        if (reader_result == get_nil()) {
            pop_op();
            break;
        }
        eval(reader_result);
        auto expr_result = get_op0();
        result.set(expr_result);
        pop_op(); // expression result
        pop_op(); // reader result

        if (expr_result->type() == Value::Type::error) {
            push_op(expr_result);
            on_error(*expr_result);
            pop_op();
            break;
        }
    }

    --bound_context->interp_entry_count_;

    return result;
}


void format_impl(Value* value, Printer& p, int depth)
{
    bool prefix_quote = false;

    switch ((lisp::Value::Type)value->type()) {
    case lisp::Value::Type::heap_node:
        // We should never reach here.
        bound_context->pfrm_.fatal("direct access to heap node");
        break;

    case lisp::Value::Type::nil:
        if (depth == 0) {
            p.put_str("'()");
        } else {
            p.put_str("()");
        }

        break;

    case lisp::Value::Type::__reserved:
        break;

    case lisp::Value::Type::character:
        // TODO...
        break;

    case lisp::Value::Type::string:
        p.put_str("\"");
        p.put_str(value->string().value());
        p.put_str("\"");
        break;

    case lisp::Value::Type::symbol:
        p.put_str(value->symbol().name_);
        break;

    case lisp::Value::Type::integer: {
        p.put_str(to_string<32>(value->integer().value_).c_str());
        break;
    }

    case lisp::Value::Type::cons:
        if (depth == 0 and not prefix_quote) {
            p.put_str("'");
            prefix_quote = true;
        }
        p.put_str("(");
        format_impl(value->cons().car(), p, depth + 1);
        if (value->cons().cdr()->type() == Value::Type::nil) {
            // ...
        } else if (value->cons().cdr()->type() not_eq Value::Type::cons) {
            p.put_str(" . ");
            format_impl(value->cons().cdr(), p, depth + 1);
        } else {
            auto current = value;
            while (true) {
                if (current->cons().cdr()->type() == Value::Type::cons) {
                    p.put_str(" ");
                    format_impl(
                        current->cons().cdr()->cons().car(), p, depth + 1);
                    current = current->cons().cdr();
                } else if (current->cons().cdr() not_eq get_nil()) {
                    p.put_str(" ");
                    format_impl(current->cons().cdr(), p, depth + 1);
                    break;
                } else {
                    break;
                }
            }
        }
        p.put_str(")");
        break;

    case lisp::Value::Type::function:
        p.put_str("<lambda>");
        break;

    case lisp::Value::Type::user_data:
        p.put_str("<ud>");
        break;

    case lisp::Value::Type::error:
        p.put_str("[ERR: ");
        p.put_str(lisp::Error::get_string(value->error().code_));
        p.put_str(" : ");
        format_impl(dcompr(value->error().context_), p, 0);
        p.put_str("]");
        break;

    case lisp::Value::Type::data_buffer:
        p.put_str("<sbr>");
        break;

    case lisp::Value::Type::count:
        break;
    }
}


const char* String::value()
{
    return dcompr(data_buffer_)->data_buffer().value()->data_ + offset_;
}


void format(Value* value, Printer& p)
{
    format_impl(value, p, 0);
}


// Garbage Collection:
//
// Each object already contains a mark bit. We will need to trace the global
// variable table and the operand stack, and deal with all of the gc
// roots. Then, we'll need to scan through the raw slab of memory allocated
// toward each memory pool used for lisp::Value instances (not the
// freelist!). For any cell in the pool with an unset mark bit, we'll add that
// node back to the pool.


static void gc_mark_value(Value* value)
{
    if (value->hdr_.mark_bit_) {
        return;
    }

    switch (value->type()) {
    case Value::Type::function:
        if (value->hdr_.mode_bits_ == Function::ModeBits::lisp_function) {
            gc_mark_value((dcompr(value->function().lisp_impl_.code_)));
            gc_mark_value(
                (dcompr(value->function().lisp_impl_.lexical_bindings_)));
        } else if (value->hdr_.mode_bits_ ==
                   Function::ModeBits::lisp_bytecode_function) {
            gc_mark_value((dcompr(value->function().bytecode_impl_.bytecode_)));
            gc_mark_value(
                (dcompr(value->function().bytecode_impl_.lexical_bindings_)));
        }
        break;

    case Value::Type::string:
        gc_mark_value(dcompr(value->string().data_buffer_));
        break;

    case Value::Type::error:
        gc_mark_value(dcompr(value->error().context_));
        break;

    case Value::Type::cons:
        if (value->cons().cdr()->type() == Value::Type::cons) {
            auto current = value;

            while (current->cons().cdr()->type() == Value::Type::cons) {
                gc_mark_value(current->cons().car());
                current = current->cons().cdr();
                current->hdr_.mark_bit_ = true;
            }

            gc_mark_value(current->cons().car());
            gc_mark_value(current->cons().cdr());

        } else {
            gc_mark_value(value->cons().car());
            gc_mark_value(value->cons().cdr());
        }
        break;

    default:
        break;
    }

    value->hdr_.mark_bit_ = true;
}


static ProtectedBase* __protected_values = nullptr;


ProtectedBase::ProtectedBase() : next_(nullptr)
{
    auto plist = __protected_values;
    if (plist) {
        plist->next_ = this;
        prev_ = plist;
    } else {
        prev_ = nullptr;
    }
    plist = this;
}

ProtectedBase::~ProtectedBase()
{
    if (next_) {
        next_->prev_ = prev_;
    }
    if (prev_) {
        prev_->next_ = next_;
    }
}


void Protected::gc_mark()
{
    gc_mark_value(val_);
}


static void gc_mark()
{
    gc_mark_value(bound_context->nil_);
    gc_mark_value(bound_context->oom_);
    gc_mark_value(bound_context->lexical_bindings_);
    gc_mark_value(bound_context->macros_);

    auto& ctx = bound_context;

    for (auto elem : *ctx->operand_stack_) {
        gc_mark_value(elem);
    }

    globals_tree_traverse(ctx->globals_tree_, [](Value& car, Value& node) {
        node.hdr_.mark_bit_ = true;
        node.cons().cdr()->hdr_.mark_bit_ = true;
        gc_mark_value(&car);
    });

    gc_mark_value(ctx->this_);

    auto p_list = __protected_values;
    while (p_list) {
        p_list->gc_mark();
        p_list = p_list->next();
    }
}


using Finalizer = void (*)(Value*);

struct FinalizerTableEntry {
    FinalizerTableEntry(Finalizer fn) : fn_(fn)
    {
    }

    Finalizer fn_;
};


static void invoke_finalizer(Value* value)
{
    // NOTE: This ordering should match the Value::Type enum.
    static const std::array<FinalizerTableEntry, Value::Type::count> table = {
        HeapNode::finalizer,
        Nil::finalizer,
        Integer::finalizer,
        Cons::finalizer,
        Function::finalizer,
        Error::finalizer,
        Symbol::finalizer,
        UserData::finalizer,
        DataBuffer::finalizer,
        String::finalizer,
        Character::finalizer,
        __Reserved::finalizer,
    };

    table[value->type()].fn_(value);
}


void DataBuffer::finalizer(Value* buffer)
{
    reinterpret_cast<ScratchBufferPtr*>(buffer->data_buffer().sbr_mem_)
        ->~ScratchBufferPtr();
}


static int gc_sweep()
{
    if (not bound_context->string_buffer_->hdr_.mark_bit_) {
        bound_context->string_buffer_ = L_NIL;
    }

    int collect_count = 0;

    for (int i = 0; i < VALUE_POOL_SIZE; ++i) {

        Value* val = (Value*)&value_pool_data[i];

        if (val->hdr_.alive_) {
            if (val->hdr_.mark_bit_) {
                val->hdr_.mark_bit_ = false;
            } else {
                invoke_finalizer(val);
                value_pool_free(val);
                ++collect_count;
            }
        }
    }

    return collect_count;
}


void live_values(::Function<24, void(Value&)> callback)
{
    for (int i = 0; i < VALUE_POOL_SIZE; ++i) {

        Value* val = (Value*)&value_pool_data[i];

        if (val->hdr_.alive_) {
            callback(*val);
        }
    }
}


static int run_gc()
{
    return gc_mark(), gc_sweep();
}


using EvalBuffer = StringBuffer<900>;


namespace {
class EvalPrinter : public Printer {
public:
    EvalPrinter(EvalBuffer& buffer) : buffer_(buffer)
    {
    }

    void put_str(const char* str) override
    {
        buffer_ += str;
    }

private:
    EvalBuffer& buffer_;
};
} // namespace


template <typename F> void foreach_string_intern(F&& fn)
{
    char* const interns = *bound_context->interns_;
    char* str = interns;

    while (static_cast<u32>(str - interns) < string_intern_table_size and
           static_cast<s32>(str - interns) <
               bound_context->string_intern_pos_ and
           *str not_eq '\0') {

        fn(str);

        str += str_len(str) + 1;
    }
}


static u32 read_list(const char* code)
{
    int i = 0;

    auto result = get_nil();
    push_op(get_nil());

    bool dotted_pair = false;

    while (true) {
        switch (code[i]) {
        case '\r':
        case '\n':
        case '\t':
        case ' ':
            ++i;
            break;

        case '.':
            i += 1;
            if (dotted_pair or result == get_nil()) {
                push_op(lisp::make_error(Error::Code::mismatched_parentheses,
                                         L_NIL));
                return i;
            } else {
                dotted_pair = true;
                i += read(code + i);
                result->cons().set_cdr(get_op0());
                pop_op();
            }
            break;

        case ';':
            while (true) {
                if (code[i] == '\0' or code[i] == '\r' or code[i] == '\n') {
                    break;
                } else {
                    ++i;
                }
            }
            break;

        case ']':
        case ')':
            ++i;
            return i;

        case '\0':
            pop_op();
            push_op(
                lisp::make_error(Error::Code::mismatched_parentheses, L_NIL));
            return i;
            break;

        default:
            if (dotted_pair) {
                push_op(lisp::make_error(Error::Code::mismatched_parentheses,
                                         L_NIL));
                return i;
            }
            i += read(code + i);

            if (result == get_nil()) {
                result = make_cons(get_op0(), get_nil());
                pop_op(); // the result from read()
                pop_op(); // nil
                push_op(result);
            } else {
                auto next = make_cons(get_op0(), get_nil());
                pop_op();
                result->cons().set_cdr(next);
                result = next;
            }
            break;
        }
    }
}


static u32 read_string(const char* code)
{
    auto temp = bound_context->pfrm_.make_scratch_buffer();
    auto write = temp->data_;

    int i = 0;
    while (*code not_eq '"') {
        if (*code == '\0' or i == SCRATCH_BUFFER_SIZE - 1) {
            // FIXME: correct error code.
            push_op(
                lisp::make_error(Error::Code::mismatched_parentheses, L_NIL));
        }
        *(write++) = *(code++);
        i++;
    }

    if (*code == '"') {
        ++i;
        ++code;
    }

    push_op(make_string(bound_context->pfrm_, temp->data_));

    return i;
}


static u32 read_symbol(const char* code)
{
    int i = 0;

    StringBuffer<64> symbol;

    if (code[0] == '\'' or code[0] == '`' or code[0] == ',' or code[0] == '@') {
        symbol.push_back(code[0]);
        push_op(make_symbol(symbol.c_str()));
        return 1;
    }

    while (true) {
        switch (code[i]) {
        case '[':
        case ']':
        case '(':
        case ')':
        case ' ':
        case '\r':
        case '\n':
        case '\t':
        case '\0':
        case ';':
            goto FINAL;

        default:
            symbol.push_back(code[i++]);
            break;
        }
    }

FINAL:

    if (symbol == "nil") {
        push_op(get_nil());
    } else {
        push_op(make_symbol(symbol.c_str()));
    }

    return i;
}


static u32 read_number(const char* code)
{
    int i = 0;

    StringBuffer<64> num_str;

    while (true) {
        switch (code[i]) {
        case 'x':
        case 'a':
        case 'b':
        case 'c':
        case 'd':
        case 'e':
        case 'f':
        case '0':
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':
            num_str.push_back(code[i++]);
            break;

        default:
            goto FINAL;
        }
    }

FINAL:

    if (num_str.length() > 1 and num_str[1] == 'x') {
        lisp::push_op(
            lisp::make_integer(hexdec((const u8*)num_str.begin() + 2)));
    } else {
        s32 result = 0;
        for (u32 i = 0; i < num_str.length(); ++i) {
            result = result * 10 + (num_str[i] - '0');
        }

        lisp::push_op(lisp::make_integer(result));
    }

    return i;
}


static void eval_let(Value* code);


static void macroexpand();


// Argument: list on operand stack
// result: list on operand stack
static void macroexpand_macro()
{
    // Ok, so this warrants some explanation: When calling this function, we've
    // just expanded a macro, but the macro expansion itself may contain macros,
    // so we'll want to iterate through the expanded expression and expand any
    // nested macros.

    // FIXME: If the macro has no nested macro expressions, we create a whole
    // bunch of garbage. Not an actual issue, but puts pressure on the gc.

    ListBuilder result;

    auto lat = get_op0();
    for (; lat not_eq get_nil(); lat = lat->cons().cdr()) {
        if (is_list(lat->cons().car())) {
            push_op(lat->cons().car());
            macroexpand_macro();
            macroexpand();
            result.push_back(get_op0());
            pop_op();
        } else {
            result.push_back(lat->cons().car());
        }
    }

    pop_op();
    push_op(result.result());
}


// Argument: list on operand stack
// result: list on operand stack
static void macroexpand()
{
    // NOTE: I know this function looks complicated. But it's really not too
    // bad.

    auto lat = get_op0();

    if (lat->cons().car()->type() == Value::Type::symbol) {

        auto macros = bound_context->macros_;
        for (; macros not_eq get_nil(); macros = macros->cons().cdr()) {

            // if Symbol matches?
            if (macros->cons().car()->cons().car()->symbol().name_ ==
                lat->cons().car()->symbol().name_) {

                auto supplied_macro_args = lat->cons().cdr();

                auto macro = macros->cons().car()->cons().cdr();
                auto macro_args = macro->cons().car();

                if (length(macro_args) > length(supplied_macro_args)) {
                    pop_op();
                    push_op(make_error(Error::Code::invalid_syntax,
                                       make_string(bound_context->pfrm_,
                                                   "invalid arguments "
                                                   "passed to marcro")));
                    return;
                }

                Protected quote(make_symbol("'"));

                // Ok, so I should explain what's going on here. For code reuse
                // purposes, we basically generate a let expression from the
                // macro parameter list, which binds the quoted macro arguments
                // to the unevaluated macro parameters.
                //
                // So, (macro foo (a b c) ...),
                // instantiated as (foo (+ 1 2) 5 6) becomes:
                //
                // (let ((a '(+ 1 2)) (b '5) (c '(6))) ...)
                //
                // Then, we just eval the let expression.
                // NOTE: The final macro argument will _always_ be a list. We
                // need to allow for variadic arguments in macro expressions,
                // and for the sake of generality, we may as well make the final
                // parameter in a macro a list in all cases, if it will need to
                // be a list in some cases.

                ListBuilder builder;
                while (macro_args not_eq get_nil()) {

                    ListBuilder assoc;

                    if (macro_args->cons().cdr() == get_nil()) {

                        assoc.push_front(make_cons(quote, supplied_macro_args));

                    } else {

                        assoc.push_front(make_cons(
                            quote, supplied_macro_args->cons().car()));
                    }

                    assoc.push_front(macro_args->cons().car());
                    builder.push_back(assoc.result());

                    macro_args = macro_args->cons().cdr();
                    supplied_macro_args = supplied_macro_args->cons().cdr();
                }

                ListBuilder synthetic_let;
                synthetic_let.push_front(macro->cons().cdr()->cons().car());
                synthetic_let.push_front(builder.result());

                eval_let(synthetic_let.result());

                auto result = get_op0();
                pop_op(); // result of eval_let()
                pop_op(); // input list
                push_op(result);

                // OK, so... we want to allow users to recursively instantiate
                // macros, so we aren't done!
                macroexpand_macro();
                return;
            } else {
                // ... no match ...
            }
        }
    }
}


u32 read(const char* code)
{
    int i = 0;

    push_op(get_nil());

    while (true) {
        switch (code[i]) {
        case '\0':
            return i;

        case '[':
        case '(':
            ++i;
            pop_op(); // nil
            i += read_list(code + i);
            macroexpand();
            // list now at stack top.
            return i;

        case ';':
            while (true) {
                if (code[i] == '\0' or code[i] == '\r' or code[i] == '\n') {
                    break;
                } else {
                    ++i;
                }
            }
            break;

        case '-':
            if (code[i + 1] >= '0' and code[i + 1] <= '9') {
                ++i;
                pop_op(); // nil
                i += read_number(code + i);
                get_op0()->integer().value_ *= -1;
                return i;
            } else {
                goto READ_SYMBOL;
            }
            break;

        case '0':
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':
            pop_op(); // nil
            i += read_number(code + i);
            // number now at stack top.
            return i;

        case '\n':
        case '\r':
        case '\t':
        case ' ':
            ++i;
            break;

        case '"':
            pop_op(); // nil
            i += read_string(code + i + 1);
            return i + 1;

        READ_SYMBOL:
        default:
            pop_op(); // nil
            i += read_symbol(code + i);
            // symbol now at stack top.

            // Ok, so for quoted expressions, we're going to put the value into
            // a cons, where the car holds the quote symbol, and the cdr holds
            // the value. Not sure how else to support top-level quoted
            // values outside of s-expressions.
            if (get_op0()->type() == Value::Type::symbol and
                (str_cmp(get_op0()->symbol().name_, "'") == 0 or
                 str_cmp(get_op0()->symbol().name_, "`") == 0)) {

                auto pair = make_cons(get_op0(), get_nil());
                push_op(pair);
                i += read(code + i);
                pair->cons().set_cdr(get_op0());
                pop_op(); // result of read()
                pop_op(); // pair
                pop_op(); // symbol
                push_op(pair);
            }
            return i;
        }
    }
}


static void eval_let(Value* code)
{
    // Overview:
    // Push the previous values of all of the let binding vars onto the stack.
    // Overwrite the current contents of the global vars. Pop the previous
    // contents off of the operand stack, and re-assign the var to the stashed
    // value.

    if (code->type() not_eq Value::Type::cons) {
        push_op(lisp::make_error(Error::Code::mismatched_parentheses, L_NIL));
        return;
    }

    Value* bindings = code->cons().car();

    Protected result(get_nil());

    {
        ListBuilder binding_list_builder;

        foreach (bindings, [&](Value* val) {
            if (result not_eq get_nil()) {
                return;
            }
            if (val->type() == Value::Type::cons) {
                auto sym = val->cons().car();
                auto bind = val->cons().cdr();
                if (sym->type() == Value::Type::symbol and
                    bind->type() == Value::Type::cons) {

                    eval(bind->cons().car());
                    binding_list_builder.push_back(make_cons(sym, get_op0()));

                    pop_op();

                } else {
                    result = lisp::make_error(
                        Error::Code::mismatched_parentheses, L_NIL);
                }
            } else {
                result = lisp::make_error(Error::Code::mismatched_parentheses,
                                          L_NIL);
            }
        })
            ;

        if (result not_eq get_nil()) {
            push_op(result);
            return;
        }

        auto new_binding_list = make_cons(binding_list_builder.result(),
                                          bound_context->lexical_bindings_);

        if (new_binding_list->type() == Value::Type::error) {
            push_op(new_binding_list);
            return;
        } else {
            bound_context->lexical_bindings_ = new_binding_list;
        }
    }

    foreach (code->cons().cdr(), [&](Value* val) {
        eval(val);
        result.set(get_op0());
        pop_op();
    })
        ;

    bound_context->lexical_bindings_ =
        bound_context->lexical_bindings_->cons().cdr();

    push_op(result);
}


static void eval_macro(Value* code)
{
    if (code->cons().car()->type() == Value::Type::symbol) {
        bound_context->macros_ = make_cons(code, bound_context->macros_);
        push_op(get_nil());
    } else {
        // TODO: raise error!
        bound_context->pfrm_.fatal("invalid macro format");
    }
}


static void eval_if(Value* code)
{
    if (code->type() not_eq Value::Type::cons) {
        push_op(lisp::make_error(Error::Code::mismatched_parentheses, L_NIL));
        return;
    }

    auto cond = code->cons().car();

    auto true_branch = get_nil();
    auto false_branch = get_nil();

    if (code->cons().cdr()->type() == Value::Type::cons) {
        true_branch = code->cons().cdr()->cons().car();

        if (code->cons().cdr()->cons().cdr()->type() == Value::Type::cons) {
            false_branch = code->cons().cdr()->cons().cdr()->cons().car();
        }
    }

    eval(cond);
    if (is_boolean_true(get_op0())) {
        eval(true_branch);
    } else {
        eval(false_branch);
    }

    auto result = get_op0();
    pop_op(); // result
    pop_op(); // cond
    push_op(result);
}


static void eval_lambda(Value* code)
{
    // todo: argument list...

    push_op(make_lisp_function(code));
}


static void eval_quasiquote(Value* code)
{
    ListBuilder builder;

    while (code not_eq get_nil()) {
        if (code->cons().car()->type() == Value::Type::symbol and
            str_cmp(code->cons().car()->symbol().name_, ",") == 0) {

            code = code->cons().cdr();

            if (code == get_nil()) {
                push_op(make_error(
                    Error::Code::invalid_syntax,
                    make_string(bound_context->pfrm_, "extraneous unquote")));
                return;
            }

            if (code->cons().car()->type() == Value::Type::symbol and
                str_cmp(code->cons().car()->symbol().name_, "@") == 0) {

                code = code->cons().cdr(); // skip over @ symbol

                eval(code->cons().car());
                auto result = get_op0();

                if (is_list(result)) {
                    // Quote splicing
                    while (result not_eq get_nil()) {
                        builder.push_back(result->cons().car());
                        result = result->cons().cdr();
                    }
                } else {
                    builder.push_back(result);
                }

                pop_op(); // result

            } else {
                eval(code->cons().car());
                auto result = get_op0();
                pop_op();

                builder.push_back(result);
            }

        } else {
            if (is_list(code->cons().car())) {
                // NOTE: because we need to expand unquotes in nested lists.
                eval_quasiquote(code->cons().car());
                builder.push_back(get_op0());
                pop_op();
            } else {
                builder.push_back(code->cons().car());
            }
        }

        code = code->cons().cdr();
    }

    push_op(builder.result());
}


void eval(Value* code)
{
    ++bound_context->interp_entry_count_;

    // NOTE: just to protect this from the GC, in case the user didn't bother to
    // do so.
    push_op(code);

    if (code->type() == Value::Type::symbol) {
        pop_op();
        push_op(get_var(code));
    } else if (code->type() == Value::Type::cons) {
        auto form = code->cons().car();
        if (form->type() == Value::Type::symbol) {
            if (str_cmp(form->symbol().name_, "if") == 0) {
                eval_if(code->cons().cdr());
                auto result = get_op0();
                pop_op(); // result
                pop_op(); // code
                push_op(result);
                --bound_context->interp_entry_count_;
                return;
            } else if (str_cmp(form->symbol().name_, "lambda") == 0) {
                eval_lambda(code->cons().cdr());
                auto result = get_op0();
                pop_op(); // result
                pop_op(); // code
                push_op(result);
                --bound_context->interp_entry_count_;
                return;
            } else if (str_cmp(form->symbol().name_, "'") == 0) {
                pop_op(); // code
                push_op(code->cons().cdr());
                --bound_context->interp_entry_count_;
                return;
            } else if (str_cmp(form->symbol().name_, "`") == 0) {
                eval_quasiquote(code->cons().cdr());
                auto result = get_op0();
                pop_op(); // result
                pop_op(); // code
                push_op(result);
                --bound_context->interp_entry_count_;
                return;
            } else if (str_cmp(form->symbol().name_, "let") == 0) {
                eval_let(code->cons().cdr());
                auto result = get_op0();
                pop_op();
                pop_op();
                push_op(result);
                --bound_context->interp_entry_count_;
                return;
            } else if (str_cmp(form->symbol().name_, "macro") == 0) {
                eval_macro(code->cons().cdr());
                pop_op();
                // TODO: store macro!
                --bound_context->interp_entry_count_;
                return;
            }
        }

        eval(code->cons().car());
        auto function = get_op0();
        pop_op();

        int argc = 0;

        auto clear_args = [&] {
            while (argc) {
                pop_op();
                --argc;
            }
        };

        auto arg_list = code->cons().cdr();
        while (true) {
            if (arg_list == get_nil()) {
                break;
            }
            if (arg_list->type() not_eq Value::Type::cons) {
                clear_args();
                pop_op();
                push_op(make_error(Error::Code::value_not_callable, arg_list));
                --bound_context->interp_entry_count_;
                return;
            }

            eval(arg_list->cons().car());
            ++argc;

            arg_list = arg_list->cons().cdr();
        }

        funcall(function, argc);
        auto result = get_op0();
        if (result->type() == Value::Type::error and
            dcompr(result->error().context_) == L_NIL) {
            result->error().context_ = compr(code);
        }
        pop_op(); // result
        pop_op(); // protected expr (see top)
        push_op(result);
        --bound_context->interp_entry_count_;
        return;
    }
}


Platform* interp_get_pfrm()
{
    return &bound_context->pfrm_;
}


void init(Platform& pfrm)
{
    if (bound_context) {
        return;
    }

    bound_context.emplace(pfrm);

    value_pool_init();
    bound_context->nil_ = alloc_value();
    bound_context->nil_->hdr_.type_ = Value::Type::nil;
    bound_context->globals_tree_ = bound_context->nil_;
    bound_context->this_ = bound_context->nil_;
    bound_context->lexical_bindings_ = bound_context->nil_;

    bound_context->oom_ = alloc_value();
    bound_context->oom_->hdr_.type_ = Value::Type::error;
    bound_context->oom_->error().code_ = Error::Code::out_of_memory;
    bound_context->oom_->error().context_ = compr(bound_context->nil_);

    bound_context->string_buffer_ = bound_context->nil_;
    bound_context->macros_ = bound_context->nil_;


    // Push a few nil onto the operand stack. Allows us to access the first few
    // elements of the operand stack without performing size checks.
    push_op(get_nil());
    push_op(get_nil());

    if (dcompr(compr(get_nil())) not_eq get_nil()) {
        bound_context->pfrm_.fatal("pointer compression test failed");
    }

    lisp::set_var("*pfrm*", lisp::make_userdata(&pfrm));

    intern("'");

    set_var("set", make_function([](int argc) {
                L_EXPECT_ARGC(argc, 2);
                L_EXPECT_OP(1, symbol);

                lisp::set_var(get_op1(), get_op0());

                return L_NIL;
            }));

    set_var("cons", make_function([](int argc) {
                L_EXPECT_ARGC(argc, 2);
                auto car = get_op1();
                auto cdr = get_op0();

                if (car->type() == lisp::Value::Type::error) {
                    return car;
                }

                if (cdr->type() == lisp::Value::Type::error) {
                    return cdr;
                }

                return make_cons(get_op1(), get_op0());
            }));

    set_var("car", make_function([](int argc) {
                L_EXPECT_ARGC(argc, 1);
                L_EXPECT_OP(0, cons);
                return get_op0()->cons().car();
            }));

    set_var("cdr", make_function([](int argc) {
                L_EXPECT_ARGC(argc, 1);
                L_EXPECT_OP(0, cons);
                return get_op0()->cons().cdr();
            }));

    set_var("list", make_function([](int argc) {
                auto lat = make_list(argc);
                for (int i = 0; i < argc; ++i) {
                    auto val = get_op((argc - 1) - i);
                    if (val->type() == Value::Type::error) {
                        return val;
                    }
                    set_list(lat, i, val);
                }
                return lat;
            }));

    set_var("arg", make_function([](int argc) {
                L_EXPECT_ARGC(argc, 1);
                L_EXPECT_OP(0, integer);
                return get_arg(get_op0()->integer().value_);
            }));

    set_var(
        "progn", make_function([](int argc) {
            // I could have defined progn at the language level, but because all of
            // the expressions are evaluated anyway, much easier to define progn as
            // a function.
            //
            // Drawbacks: (1) Defining progn takes up a small amount of memory for
            // the function object. (2) Extra use of the operand stack.
            return get_op0();
        }));

    set_var("any-true", make_function([](int argc) {
                for (int i = 0; i < argc; ++i) {
                    if (is_boolean_true(get_op(i))) {
                        return get_op(i);
                    }
                }
                return L_NIL;
            }));

    set_var("all-true", make_function([](int argc) {
                for (int i = 0; i < argc; ++i) {
                    if (not is_boolean_true(get_op(i))) {
                        return L_NIL;
                    }
                }
                return make_integer(1);
            }));


    set_var("not", make_function([](int argc) {
                L_EXPECT_ARGC(argc, 1);
                return make_integer(not is_boolean_true(get_op0()));
            }));

    set_var(
        "equal", make_function([](int argc) {
            L_EXPECT_ARGC(argc, 2);

            if (get_op0()->type() not_eq get_op1()->type()) {
                return make_integer(0);
            }

            return make_integer([] {
                switch (get_op0()->type()) {
                case Value::Type::integer:
                    return get_op0()->integer().value_ ==
                           get_op1()->integer().value_;

                case Value::Type::cons:
                    // TODO!
                    // This comparison needs to be done as efficiently as possible...
                    break;

                case Value::Type::count:
                case Value::Type::__reserved:
                case Value::Type::character:
                case Value::Type::nil:
                case Value::Type::heap_node:
                case Value::Type::data_buffer:
                case Value::Type::function:
                    return get_op0() == get_op1();

                case Value::Type::error:
                    break;

                case Value::Type::symbol:
                    return get_op0()->symbol().name_ ==
                           get_op1()->symbol().name_;

                case Value::Type::user_data:
                    return get_op0()->user_data().obj_ ==
                           get_op1()->user_data().obj_;

                case Value::Type::string:
                    return str_cmp(get_op0()->string().value(),
                                   get_op1()->string().value()) == 0;
                }
                return false;
            }());
        }));

    set_var("apply", make_function([](int argc) {
                L_EXPECT_ARGC(argc, 2);
                L_EXPECT_OP(0, cons);
                L_EXPECT_OP(1, function);

                auto lat = get_op0();
                auto fn = get_op1();

                int apply_argc = 0;
                while (lat not_eq get_nil()) {
                    if (lat->type() not_eq Value::Type::cons) {
                        return make_error(Error::Code::invalid_argument_type,
                                          lat);
                    }
                    ++apply_argc;
                    push_op(lat->cons().car());

                    lat = lat->cons().cdr();
                }

                funcall(fn, apply_argc);

                auto result = get_op0();
                pop_op();

                return result;
            }));

    set_var("fill", make_function([](int argc) {
                L_EXPECT_ARGC(argc, 2);
                L_EXPECT_OP(1, integer);

                auto result = make_list(get_op1()->integer().value_);
                for (int i = 0; i < get_op1()->integer().value_; ++i) {
                    set_list(result, i, get_op0());
                }

                return result;
            }));

    set_var("gen", make_function([](int argc) {
                L_EXPECT_ARGC(argc, 2);
                L_EXPECT_OP(1, integer);

                auto result = make_list(get_op1()->integer().value_);
                auto fn = get_op0();
                const int count = get_op1()->integer().value_;
                push_op(result);
                for (int i = 0; i < count; ++i) {
                    push_op(make_integer(i));
                    funcall(fn, 1);
                    set_list(result, i, get_op0());
                    pop_op(); // result from funcall
                }
                pop_op(); // result
                return result;
            }));

    set_var("length", make_function([](int argc) {
                L_EXPECT_ARGC(argc, 1);

                if (get_op0()->type() == Value::Type::nil) {
                    return make_integer(0);
                }

                L_EXPECT_OP(0, cons);

                return make_integer(length(get_op0()));
            }));

    set_var("<", make_function([](int argc) {
                L_EXPECT_ARGC(argc, 2);
                L_EXPECT_OP(0, integer);
                L_EXPECT_OP(1, integer);
                return make_integer(get_op1()->integer().value_ <
                                    get_op0()->integer().value_);
            }));

    set_var(">", make_function([](int argc) {
                L_EXPECT_ARGC(argc, 2);
                L_EXPECT_OP(0, integer);
                L_EXPECT_OP(1, integer);
                return make_integer(get_op1()->integer().value_ >
                                    get_op0()->integer().value_);
            }));

    set_var("+", make_function([](int argc) {
                int accum = 0;
                for (int i = 0; i < argc; ++i) {
                    L_EXPECT_OP(i, integer);
                    accum += get_op(i)->integer().value_;
                }
                return make_integer(accum);
            }));

    set_var("-", make_function([](int argc) {
                L_EXPECT_ARGC(argc, 2);
                L_EXPECT_OP(1, integer);
                L_EXPECT_OP(0, integer);
                return make_integer(get_op1()->integer().value_ -
                                    get_op0()->integer().value_);
            }));

    set_var("*", make_function([](int argc) {
                int accum = 1;
                for (int i = 0; i < argc; ++i) {
                    L_EXPECT_OP(i, integer);
                    accum *= get_op(i)->integer().value_;
                }
                return make_integer(accum);
            }));

    set_var("/", make_function([](int argc) {
                L_EXPECT_ARGC(argc, 2);
                L_EXPECT_OP(1, integer);
                L_EXPECT_OP(0, integer);
                return make_integer(get_op1()->integer().value_ /
                                    get_op0()->integer().value_);
            }));

    set_var("interp-stat", make_function([](int argc) {
                auto& ctx = bound_context;

                int values_remaining = 0;
                Value* current = value_pool;
                while (current) {
                    ++values_remaining;
                    current = current->heap_node().next_;
                }

                ListBuilder lat;

                auto make_stat = [&](const char* name, int value) {
                    auto c = make_cons(get_nil(), get_nil());
                    if (c == bound_context->oom_) {
                        return c;
                    }
                    push_op(c); // gc protect

                    c->cons().set_car(
                        make_symbol(name, Symbol::ModeBits::stable_pointer));
                    c->cons().set_cdr(make_integer(value));

                    pop_op(); // gc unprotect
                    return c;
                };

                lat.push_front(make_stat("vars", [&] {
                    int symb_tab_used = 0;
                    globals_tree_traverse(
                        ctx->globals_tree_,
                        [&symb_tab_used](Value&, Value&) { ++symb_tab_used; });
                    return symb_tab_used;
                }()));

                lat.push_front(make_stat("stk", ctx->operand_stack_->size()));
                lat.push_front(make_stat("internb", ctx->string_intern_pos_));
                lat.push_front(make_stat("free", values_remaining));

                int databuffers = 0;

                for (int i = 0; i < VALUE_POOL_SIZE; ++i) {
                    Value* val = (Value*)&value_pool_data[i];
                    if (val->hdr_.alive_ and
                        val->hdr_.type_ == Value::Type::data_buffer) {
                        ++databuffers;
                    }
                }

                lat.push_front(make_stat("sbr", databuffers));

                return lat.result();
            }));

    set_var("range", make_function([](int argc) {
                int start = 0;
                int end = 0;
                int incr = 1;

                if (argc == 1) {

                    L_EXPECT_OP(0, integer);

                    start = 0;
                    end = get_op0()->integer().value_;

                } else if (argc == 2) {

                    L_EXPECT_OP(1, integer);
                    L_EXPECT_OP(0, integer);

                    start = get_op1()->integer().value_;
                    end = get_op0()->integer().value_;

                } else if (argc == 3) {

                    L_EXPECT_OP(2, integer);
                    L_EXPECT_OP(1, integer);
                    L_EXPECT_OP(0, integer);

                    start = get_op(2)->integer().value_;
                    end = get_op1()->integer().value_;
                    incr = get_op0()->integer().value_;
                } else {
                    return lisp::make_error(lisp::Error::Code::invalid_argc,
                                            L_NIL);
                }

                if (incr == 0) {
                    return get_nil();
                }

                ListBuilder lat;

                for (int i = start; i < end; i += incr) {
                    lat.push_back(make_integer(i));
                }

                return lat.result();
            }));

    set_var("unbind", make_function([](int argc) {
                L_EXPECT_ARGC(argc, 1);
                L_EXPECT_OP(0, symbol);

                globals_tree_erase(get_op0());

                return get_nil();
            }));


    set_var("symbol", make_function([](int argc) {
                L_EXPECT_ARGC(argc, 1);
                L_EXPECT_OP(0, string);

                return make_symbol(get_op0()->string().value());
            }));

    set_var("type", make_function([](int argc) {
                L_EXPECT_ARGC(argc, 1);
                return make_symbol([] {
                    // clang-format off
                    switch (get_op0()->type()) {
            case Value::Type::nil: return "nil";
            case Value::Type::integer: return "integer";
            case Value::Type::cons: return "pair";
            case Value::Type::function: return "function";
            case Value::Type::error: return "error";
            case Value::Type::symbol: return "symbol";
            case Value::Type::user_data: return "ud";
            case Value::Type::data_buffer: return "databuffer";
            case Value::Type::string: return "string";
            case Value::Type::character: return "character";
            case Value::Type::count:
            case Value::Type::__reserved:
            case Value::Type::heap_node:
                break;
            }
                    // clang-format on
                    return "???";
                }());
            }));


    set_var("string", make_function([](int argc) {
                EvalBuffer b;
                EvalPrinter p(b);

                for (int i = argc - 1; i > -1; --i) {
                    auto val = get_op(i);
                    if (val->type() == Value::Type::string) {
                        p.put_str(val->string().value());
                    } else {
                        format_impl(val, p, 0);
                    }
                }

                if (auto pfrm = interp_get_pfrm()) {
                    return make_string(*pfrm, b.c_str());
                }
                return L_NIL;
            }));

    set_var("bound", make_function([](int argc) {
                L_EXPECT_ARGC(argc, 1);
                L_EXPECT_OP(0, symbol);

                auto found = globals_tree_find(get_op0());
                return make_integer(found not_eq get_nil() and
                                    found->type() not_eq
                                        lisp::Value::Type::error);
            }));


    set_var("filter", make_function([](int argc) {
                L_EXPECT_ARGC(argc, 2);
                L_EXPECT_OP(0, cons);
                L_EXPECT_OP(1, function);

                auto fn = get_op1();
                Value* result = make_cons(L_NIL, L_NIL);
                auto prev = result;
                auto current = result;

                foreach (get_op0(), [&](Value* val) {
                    push_op(result); // gc protect

                    push_op(val);
                    funcall(fn, 1);
                    auto funcall_result = get_op0();

                    if (is_boolean_true(funcall_result)) {
                        current->cons().set_car(val);
                        auto next = make_cons(L_NIL, L_NIL);
                        if (next == bound_context->oom_) {
                            current = result;
                            return;
                        }
                        current->cons().set_cdr(next);
                        prev = current;
                        current = next;
                    }
                    pop_op(); // funcall result

                    pop_op(); // gc unprotect
                })
                    ;

                if (current == result) {
                    return L_NIL;
                }

                prev->cons().set_cdr(L_NIL);

                return result;
            }));


    set_var(
        "map", make_function([](int argc) {
            if (argc < 2) {
                return get_nil();
            }
            if (lisp::get_op(argc - 1)->type() not_eq Value::Type::function and
                lisp::get_op(argc - 1)->type() not_eq Value::Type::cons) {
                return lisp::make_error(
                    lisp::Error::Code::invalid_argument_type, L_NIL);
            }

            // I've never seen map used with so many input lists, but who knows,
            // someone might try to call this with more than six inputs...
            Buffer<Value*, 6> inp_lats;

            if (argc < static_cast<int>(inp_lats.size())) {
                return get_nil(); // TODO: return error
            }

            for (int i = 0; i < argc - 1; ++i) {
                L_EXPECT_OP(i, cons);
                inp_lats.push_back(get_op(i));
            }

            const auto len = length(inp_lats[0]);
            if (len == 0) {
                return get_nil();
            }
            for (auto& l : inp_lats) {
                if (length(l) not_eq len) {
                    return get_nil(); // return error instead!
                }
            }

            auto fn = get_op(argc - 1);

            int index = 0;

            Value* result = make_list(len);
            push_op(result); // protect from the gc

            // Because the length function returned a non-zero value, we've
            // already succesfully scanned the list, so we don't need to do any
            // type checking.

            while (index < len) {

                for (auto& lat : reversed(inp_lats)) {
                    push_op(lat->cons().car());
                    lat = lat->cons().cdr();
                }
                funcall(fn, inp_lats.size());

                set_list(result, index, get_op0());
                pop_op();

                ++index;
            }

            pop_op(); // the protected result list

            return result;
        }));

    set_var("reverse", make_function([](int argc) {
                L_EXPECT_ARGC(argc, 1);
                L_EXPECT_OP(0, cons);

                Value* result = get_nil();
                foreach (get_op0(), [&](Value* car) {
                    push_op(result);
                    result = make_cons(car, result);
                    pop_op();
                })
                    ;

                return result;
            }));

    set_var("select", make_function([](int argc) {
                L_EXPECT_ARGC(argc, 2);
                L_EXPECT_OP(0, cons);
                L_EXPECT_OP(1, cons);

                const auto len = length(get_op0());
                if (not len or len not_eq length(get_op1())) {
                    return get_nil();
                }

                auto input_list = get_op1();
                auto selection_list = get_op0();

                auto result = get_nil();
                for (int i = len - 1; i > -1; --i) {
                    if (is_boolean_true(get_list(selection_list, i))) {
                        push_op(result);
                        auto next = make_cons(get_list(input_list, i), result);
                        result = next;
                        pop_op(); // result
                    }
                }

                return result;
            }));

    set_var("gc",
            make_function([](int argc) { return make_integer(run_gc()); }));

    set_var("get", make_function([](int argc) {
                L_EXPECT_ARGC(argc, 2);
                L_EXPECT_OP(1, cons);
                L_EXPECT_OP(0, integer);

                return get_list(get_op1(), get_op0()->integer().value_);
            }));


    set_var("read", make_function([](int argc) {
                L_EXPECT_ARGC(argc, 1);
                L_EXPECT_OP(0, string);
                read(get_op0()->string().value());
                auto result = get_op0();
                pop_op();
                return result;
            }));


    set_var("eval", make_function([](int argc) {
                if (argc < 1) {
                    return lisp::make_error(lisp::Error::Code::invalid_argc,
                                            L_NIL);
                }

                eval(get_op0());
                auto result = get_op0();
                pop_op(); // result

                return result;
            }));

    set_var("globals", make_function([](int argc) {
                return bound_context->globals_tree_;
            }));

    set_var("this",
            make_function([](int argc) { return bound_context->this_; }));

    set_var("argc", make_function([](int argc) {
                // NOTE: This works because native functions do not assign
                // current_fn_argc_.
                return make_integer(bound_context->current_fn_argc_);
            }));

    set_var("env", make_function([](int argc) {
                auto pfrm = interp_get_pfrm();

                Value* result = make_cons(get_nil(), get_nil());
                push_op(result); // protect from the gc

                Value* current = result;

                get_env([&current, pfrm](const char* str) {
                    current->cons().set_car(intern_to_symbol(str));
                    auto next = make_cons(get_nil(), get_nil());
                    if (next not_eq bound_context->oom_) {
                        current->cons().set_cdr(next);
                        current = next;
                    }
                });

                pop_op(); // result

                return result;
            }));

    set_var("compile", make_function([](int argc) {
                auto pfrm = interp_get_pfrm();

                L_EXPECT_ARGC(argc, 1);
                L_EXPECT_OP(0, function);

                if (get_op0()->hdr_.mode_bits_ ==
                    Function::ModeBits::lisp_function) {
                    compile(*pfrm,
                            dcompr(get_op0()->function().lisp_impl_.code_));
                    auto ret = get_op0();
                    pop_op();
                    return ret;
                } else {
                    return get_op0();
                }
            }));

    set_var(
        "disassemble", make_function([](int argc) {
            L_EXPECT_ARGC(argc, 1);
            L_EXPECT_OP(0, function);

            if (get_op0()->hdr_.mode_bits_ ==
                Function::ModeBits::lisp_bytecode_function) {

                Platform::RemoteConsole::Line out;

                u8 depth = 0;

                auto buffer = get_op0()->function().bytecode_impl_.databuffer();

                auto data = buffer->data_buffer().value();

                const auto start_offset = get_op0()
                                              ->function()
                                              .bytecode_impl_.bytecode_offset()
                                              ->integer()
                                              .value_;

                for (int i = start_offset; i < SCRATCH_BUFFER_SIZE;) {

                    const auto offset = to_string<10>(i - start_offset);
                    if (offset.length() < 4) {
                        for (u32 i = 0; i < 4 - offset.length(); ++i) {
                            out.push_back('0');
                        }
                    }

                    out += offset;
                    out += ": ";

                    using namespace instruction;

                    switch ((Opcode)(*data).data_[i]) {
                    case Fatal::op():
                        return get_nil();

                    case LoadVar::op():
                        i += 1;
                        out += "LOAD_VAR(";
                        out += symbol_from_offset(
                            ((HostInteger<s16>*)(data->data_ + i))->get());
                        out += ")";
                        i += 2;
                        break;

                    case LoadVarRelocatable::op():
                        i += 1;
                        out += "LOAD_VAR_RELOCATABLE(";
                        out += to_string<10>(
                            ((HostInteger<s16>*)(data->data_ + i))->get());
                        out += ")";
                        i += 2;
                        break;

                    case PushSymbol::op():
                        i += 1;
                        out += "PUSH_SYMBOL(";
                        out += symbol_from_offset(
                            ((HostInteger<s16>*)(data->data_ + i))->get());
                        out += ")";
                        i += 2;
                        break;

                    case PushSymbolRelocatable::op():
                        i += 1;
                        out += "PUSH_SYMBOL_RELOCATABLE(";
                        out += to_string<10>(
                            ((HostInteger<s16>*)(data->data_ + i))->get());
                        out += ")";
                        i += 2;
                        break;

                    case PushString::op(): {
                        i += 1;
                        out += PushString::name();
                        out += "(\"";
                        u8 len = *(data->data_ + (i++));
                        out += data->data_ + i;
                        out += "\")";
                        i += len;
                        break;
                    }

                    case PushNil::op():
                        out += "PUSH_NIL";
                        i += 1;
                        break;

                    case Push0::op():
                        i += 1;
                        out += "PUSH_0";
                        break;

                    case Push1::op():
                        i += 1;
                        out += "PUSH_1";
                        break;

                    case Push2::op():
                        i += 1;
                        out += "PUSH_2";
                        break;

                    case PushInteger::op():
                        i += 1;
                        out += "PUSH_INTEGER(";
                        out += to_string<10>(
                            ((HostInteger<s32>*)(data->data_ + i))->get());
                        out += ")";
                        i += 4;
                        break;

                    case PushSmallInteger::op():
                        out += "PUSH_SMALL_INTEGER(";
                        out += to_string<10>(*(data->data_ + i + 1));
                        out += ")";
                        i += 2;
                        break;

                    case JumpIfFalse::op():
                        out += "JUMP_IF_FALSE(";
                        out += to_string<10>(
                            ((HostInteger<u16>*)(data->data_ + i + 1))->get());
                        out += ")";
                        i += 3;
                        break;

                    case Jump::op():
                        out += "JUMP(";
                        out += to_string<10>(
                            ((HostInteger<u16>*)(data->data_ + i + 1))->get());
                        out += ")";
                        i += 3;
                        break;

                    case SmallJumpIfFalse::op():
                        out += "SMALL_JUMP_IF_FALSE(";
                        out += to_string<10>(*(data->data_ + i + 1));
                        out += ")";
                        i += 2;
                        break;

                    case SmallJump::op():
                        out += "SMALL_JUMP(";
                        out += to_string<10>(*(data->data_ + i + 1));
                        out += ")";
                        i += 2;
                        break;

                    case PushLambda::op():
                        out += "PUSH_LAMBDA(";
                        out += to_string<10>(
                            ((HostInteger<u16>*)(data->data_ + i + 1))->get());
                        out += ")";
                        i += 3;
                        ++depth;
                        break;

                    case PushThis::op():
                        out += PushThis::name();
                        i += sizeof(PushThis);
                        break;

                    case Arg::op():
                        out += Arg::name();
                        i += sizeof(Arg);
                        break;

                    case Arg0::op():
                        out += Arg0::name();
                        i += sizeof(Arg0);
                        break;

                    case Arg1::op():
                        out += Arg1::name();
                        i += sizeof(Arg1);
                        break;

                    case Arg2::op():
                        out += Arg2::name();
                        i += sizeof(Arg2);
                        break;

                    case TailCall::op():
                        out += TailCall::name();
                        out += "(";
                        out += to_string<10>(*(data->data_ + i + 1));
                        out += ")";
                        i += 2;
                        break;

                    case TailCall1::op():
                        out += TailCall1::name();
                        ++i;
                        break;

                    case TailCall2::op():
                        out += TailCall2::name();
                        ++i;
                        break;

                    case TailCall3::op():
                        out += TailCall3::name();
                        ++i;
                        break;

                    case Funcall::op():
                        out += "FUNCALL(";
                        out += to_string<10>(*(data->data_ + i + 1));
                        out += ")";
                        i += 2;
                        break;

                    case PushList::op():
                        out += "PUSH_LIST(";
                        out += to_string<10>(*(data->data_ + i + 1));
                        out += ")";
                        i += 2;
                        break;

                    case Funcall1::op():
                        out += "FUNCALL_1";
                        i += 1;
                        break;

                    case Funcall2::op():
                        out += "FUNCALL_2";
                        i += 1;
                        break;

                    case Funcall3::op():
                        out += "FUNCALL_3";
                        i += 1;
                        break;

                    case Pop::op():
                        out += "POP";
                        i += 1;
                        break;

                    case MakePair::op():
                        out += "MAKE_PAIR";
                        i += 1;
                        break;

                    case Not::op():
                        out += Not::name();
                        i += sizeof(Not);
                        break;

                    case First::op():
                        out += First::name();
                        i += sizeof(First);
                        break;

                    case Rest::op():
                        out += Rest::name();
                        i += sizeof(Rest);
                        break;

                    case Dup::op():
                        out += Dup::name();
                        i += 1;
                        break;

                    case EarlyRet::op():
                        out += EarlyRet::name();
                        i += sizeof(EarlyRet);
                        break;

                    case LexicalDef::op():
                        out += LexicalDef::name();
                        out += "(";
                        out += symbol_from_offset(
                            ((HostInteger<s16>*)(data->data_ + i + 1))->get());
                        out += ")";
                        i += sizeof(LexicalDef);
                        break;

                    case LexicalDefRelocatable::op():
                        out += LexicalDefRelocatable::name();
                        out += "(";
                        out += to_string<10>(
                            ((HostInteger<s16>*)(data->data_ + i + 1))->get());
                        out += ")";
                        i += sizeof(LexicalDefRelocatable);
                        break;

                    case LexicalFramePush::op():
                        out += LexicalFramePush::name();
                        i += sizeof(LexicalFramePush);
                        break;

                    case LexicalFramePop::op():
                        out += LexicalFramePop::name();
                        i += sizeof(LexicalFramePop);
                        break;

                    case LexicalVarLoad::op():
                        out += LexicalVarLoad::name();
                        i += sizeof(LexicalVarLoad);
                        break;

                    case Ret::op(): {
                        if (depth == 0) {
                            out += "RET\r\n";
                            auto pfrm = interp_get_pfrm();
                            pfrm->remote_console().printline(out.c_str(),
                                                             false);
                            ((Platform*)pfrm)->sleep(80);
                            return get_nil();
                        } else {
                            --depth;
                            out += "RET";
                            i += 1;
                        }
                        break;
                    }

                    default:
                        interp_get_pfrm()->remote_console().printline(
                            out.c_str(), false);
                        interp_get_pfrm()->sleep(80);
                        return get_nil();
                    }
                    out += "\r\n";
                }
                return get_nil();
            } else if (get_op0()->hdr_.mode_bits_ ==
                       Function::ModeBits::lisp_function) {
                auto expression_list =
                    dcompr(get_op0()->function().lisp_impl_.code_);

                DefaultPrinter p;
                format(expression_list, p);

                interp_get_pfrm()->remote_console().printline(p.fmt_.c_str(),
                                                              false);
                interp_get_pfrm()->sleep(80);
                return get_nil();

            } else {
                return get_nil();
            }
        }));
}


void load_module(Module* module)
{
    Protected buffer(make_databuffer(bound_context->pfrm_));
    Protected zero(make_integer(0));

    Protected bytecode(make_cons(zero, buffer));
    push_op(make_bytecode_function(bytecode)); // result on stack

    auto load_module_symbol = [&](int sym) {
        auto search = (const char*)module + sizeof(Module::Header);

        for (int i = 0;;) {
            if (sym == 0) {
                return search + i;
            } else {
                while (search[i] not_eq '\0') {
                    ++i;
                }
                ++i;
                --sym;
            }
        }
    };

    auto sbr = buffer->data_buffer().value();

    auto data = load_module_symbol(module->header_.symbol_count_.get());
    memcpy(sbr->data_, data, module->header_.bytecode_length_.get());

    int depth = 0;
    int index = 0;

    while (true) {
        auto inst = instruction::load_instruction(*sbr, index);

        switch (inst->op_) {
        case instruction::PushLambda::op():
            ++depth;
            ++index;
            break;

        case instruction::Ret::op():
            if (depth == 0) {
                return;
            }
            --depth;
            ++index;
            break;

        case instruction::LoadVarRelocatable::op(): {
            auto sym_num =
                ((instruction::LoadVarRelocatable*)inst)->name_offset_.get();
            auto str = load_module_symbol(sym_num);
            ((instruction::LoadVar*)inst)
                ->name_offset_.set(symbol_offset(intern(str)));
            inst->op_ = instruction::LoadVar::op();
            ++index;
            break;
        }

        case instruction::PushSymbolRelocatable::op(): {
            auto sym_num =
                ((instruction::PushSymbolRelocatable*)inst)->name_offset_.get();
            auto str = load_module_symbol(sym_num);
            ((instruction::PushSymbol*)inst)
                ->name_offset_.set(symbol_offset(intern(str)));
            inst->op_ = instruction::PushSymbol::op();
            ++index;
            break;
        }

        case instruction::LexicalDefRelocatable::op(): {
            auto sym_num =
                ((instruction::LexicalDefRelocatable*)inst)->name_offset_.get();
            auto str = load_module_symbol(sym_num);
            ((instruction::LexicalDef*)inst)
                ->name_offset_.set(symbol_offset(intern(str)));
            inst->op_ = instruction::LexicalDef::op();
            ++index;
            break;
        }

        default:
            ++index;
            break;
        }
    }
}


} // namespace lisp
