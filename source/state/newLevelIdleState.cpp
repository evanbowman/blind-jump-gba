#include "state_impl.hpp"


void NewLevelIdleState::receive(const net_event::NewLevelIdle&,
                                Platform& pfrm,
                                Game&)
{
    info(pfrm, "got new level idle msg");
    peer_ready_ = true;
}


void NewLevelIdleState::receive(const net_event::NewLevelSyncSeed& sync_seed,
                                Platform& pfrm,
                                Game& game)
{
    const auto peer_seed = sync_seed.random_state_.get();
    if (rng::critical_state == peer_seed) {

        if (matching_syncs_received_ == 0) {
            display_text(pfrm, LocaleString::level_transition_synchronizing);
        }

        matching_syncs_received_ += 1;

        static const int required_matching_syncs = 10;
        if (matching_syncs_received_ == required_matching_syncs) {

            // We're ready, but what if, for some reason, the other peer has one
            // or more fewer matching syncs than we do? In that case, let's spam
            // our own seed just to be sure.
            for (int i = 0; i < required_matching_syncs; ++i) {
                net_event::NewLevelSyncSeed sync_seed;
                sync_seed.random_state_.set(rng::critical_state);
                sync_seed.difficulty_ = game.difficulty();
                net_event::transmit(pfrm, sync_seed);
                pfrm.sleep(1);
            }

            ready_ = true;
        }
    } else {
        game.persistent_data().settings_.difficulty_ =
            static_cast<Settings::Difficulty>(sync_seed.difficulty_);

        rng::critical_state = peer_seed;
        matching_syncs_received_ = 0;
    }
}


void NewLevelIdleState::display_text(Platform& pfrm, LocaleString ls)
{
    const auto str = locale_string(ls);

    const auto margin = centered_text_margins(pfrm, str_len(str));

    auto screen_tiles = calc_screen_tiles(pfrm);

    text_.emplace(pfrm, OverlayCoord{(u8)margin, (u8)(screen_tiles.y / 2 - 1)});

    text_->assign(str);
}


void NewLevelIdleState::enter(Platform& pfrm, Game& game, State& prev_state)
{
    if (pfrm.network_peer().is_connected()) {
        display_text(pfrm, LocaleString::level_transition_awaiting_peers);
    }
}


void NewLevelIdleState::exit(Platform& pfrm, Game& game, State& next_state)
{
    text_.reset();
}


StatePtr
NewLevelIdleState::update(Platform& pfrm, Game& game, Microseconds delta)
{
    // Synchronization procedure for seed values at level transition:
    //
    // Players transmit NewLevelIdle messages until both players are ready. Once
    // a device receives a NewLevelIdleMessage, it starts transmitting its
    // current random seed value. Upon receiving another device's random seed
    // value, a device resets its own random seed to the received value, if the
    // seed values do not match. After receiving N matching seed values, both
    // peers should advance to the next level.

    if (pfrm.network_peer().is_connected()) {
        timer_ += delta;
        if (timer_ > milliseconds(250)) {
            info(pfrm, "transmit new level idle msg");
            timer_ -= milliseconds(250);

            net_event::NewLevelIdle idle_msg;
            idle_msg.header_.message_type_ = net_event::Header::new_level_idle;
            pfrm.network_peer().send_message(
                {(byte*)&idle_msg, sizeof idle_msg});

            if (peer_ready_) {
                net_event::NewLevelSyncSeed sync_seed;
                sync_seed.random_state_.set(rng::critical_state);
                sync_seed.difficulty_ = game.difficulty();
                net_event::transmit(pfrm, sync_seed);
                info(pfrm, "sent seed to peer");
            }
        }

        net_event::poll_messages(pfrm, game, *this);

    } else {
        ready_ = true;
    }

    if (ready_) {
        Level next_level = game.level() + 1;

        // backdoor for debugging purposes.
        if (pfrm.keyboard().all_pressed<Key::alt_1, Key::alt_2, Key::start>()) {
            return state_pool().create<CommandCodeState>();
        }


        // Boss levels still need a lot of work before enabling them for
        // multiplayer, in order to properly synchronize the bosses across
        // connected games. For simpler enemies and larger levels, we don't need
        // to be as strict about keeping the enemies perferctly
        // synchronized. But for boss fights, the bar is higher, and I'm not
        // satisfied with any of the progress so far.
        if (is_boss_level(next_level) and pfrm.network_peer().is_connected()) {
            next_level += 1;
        }

        // For now, to determine whether the game's complete, scan through a
        // bunch of levels. If there are no more bosses remaining, the game is
        // complete.
        bool bosses_remaining = false;
        for (Level l = next_level; l < next_level + 1000; ++l) {
            if (is_boss_level(l)) {
                bosses_remaining = true;
                break;
            }
        }

        auto zone = zone_info(next_level);
        auto last_zone = zone_info(game.level());

        if (not bosses_remaining and not(zone == last_zone)) {
            pfrm.sleep(120);
            return state_pool().create<EndingCreditsState>();
        }

        return state_pool().create<NewLevelState>(next_level);
    }

    return null_state();
}