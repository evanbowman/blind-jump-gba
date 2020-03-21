#include "drone.hpp"
#include "common.hpp"
#include "game.hpp"
#include "number/random.hpp"


Drone::Drone(const Vec2<Float>& pos)
    : Entity(Entity::Health(4)), state_{State::sleep},
      timer_(0), hitbox_{&position_, {16, 16}, {8, 13}}
{
    set_position(pos);

    sprite_.set_position(pos);
    sprite_.set_size(Sprite::Size::w16_h32);
    sprite_.set_origin({8, 13});
    sprite_.set_texture_index(TextureMap::drone);

    shadow_.set_position(pos);
    shadow_.set_origin({7, -12});
    shadow_.set_size(Sprite::Size::w16_h32);
    shadow_.set_alpha(Sprite::Alpha::translucent);
    shadow_.set_texture_index(TextureMap::drop_shadow);
}


void Drone::update(Platform& pfrm, Game& game, Microseconds dt)
{
    fade_color_anim_.advance(sprite_, dt);

    if (game.player().get_position().x > position_.x) {
        sprite_.set_flip({true, false});
    } else {
        sprite_.set_flip({false, false});
    }

    timer_ += dt;

    switch (state_) {
    case State::sleep:
        if (timer_ > seconds(2)) {
            timer_ = 0;
            state_ = State::inactive;
        }
        break;

    case State::inactive:
        if (visible()) {
            timer_ = 0;
            const auto screen_size = pfrm.screen().size();
            if (manhattan_length(game.player().get_position(), position_) <
                std::min(screen_size.x, screen_size.y)) {
                state_ = State::idle;
            }
        }
        break;

    case State::move:
        position_ = position_ + Float(dt) * step_vector_;
        sprite_.set_position(position_);
        shadow_.set_position(position_);
        if (timer_ > seconds(1)) {
            timer_ = 0;
            state_ = State::idle;
        }
        break;

    case State::idle:
        if (timer_ > milliseconds(700)) {
            timer_ = 0;
            state_ = State::move;
            const auto player_pos = game.player().get_position();
            const auto target = [&] {
                if (random_choice<2>()) {
                    return player_pos;
                } else {
                    return sample<64>(player_pos);
                }
            }();
            step_vector_ = direction(position_, target) * 0.000055f;
        }
        break;
    }
}


void Drone::on_collision(Platform& pf, Game& game, Laser&)
{
    sprite_.set_mix({ColorConstant::aerospace_orange, 255});
    debit_health(1);

    if (not alive()) {
        game.score() += 3;

        pf.sleep(5);

        static const Item::Type item_drop_vec[] = {Item::Type::coin,
                                                   Item::Type::null};

        on_enemy_destroyed(pf, game, position_, 7, item_drop_vec);
    }
}