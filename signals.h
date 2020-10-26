#pragma once
#include <functional>
#include <boost/intrusive/list.hpp>
#include <boost/intrusive/slist.hpp>

namespace signals
{

    template <typename T>
    struct signal;

    template <typename... Args>
    struct signal<void (Args...)>
    {
        using slot_t = std::function<void(Args...)>;

        using list_auto_unlink_hook =
                boost::intrusive::list_base_hook<
                    boost::intrusive::link_mode<
                        boost::intrusive::auto_unlink>>;

        using slist_normal_link_hook =
        boost::intrusive::slist_base_hook<
                boost::intrusive::link_mode<
                        boost::intrusive::normal_link>>;

        struct connection : list_auto_unlink_hook {
            template <typename Friend>
            friend struct signal;

            connection() noexcept;

            connection(signal *sig, slot_t slot) noexcept;

            connection(connection const& rhs) = delete;

            connection& operator=(connection const& rhs) = delete;

            connection(connection&& rhs) noexcept;

            connection& operator=(connection&& rhs) noexcept;

            ~connection() noexcept;

            void disconnect() noexcept;

            void move_forward_iteration_tokens() noexcept;

            void replace_iteration_tokens_with_this(connection& rhs) noexcept;

        private:
            signal *sig;
            slot_t slot;
        };

        using connections_t = boost::intrusive::list<
                connection, boost::intrusive::constant_time_size<false>>;

        struct iteration_token : slist_normal_link_hook {
            explicit iteration_token(typename connections_t::const_iterator current) noexcept
            : current(current), is_signal_deleted(false) {}

            typename connections_t::const_iterator current;
            bool is_signal_deleted;
        };

        using iteration_tokens_t = boost::intrusive::slist<
                iteration_token, boost::intrusive::constant_time_size<false>>;

        signal() = default;

        signal(signal const&) = delete;
        signal& operator=(signal const&) = delete;

        signal(signal&&) noexcept = default;
        signal& operator=(signal&&) noexcept = default;

        ~signal() noexcept;

        connection connect(std::function<void (Args...)> slot) noexcept;

        void operator()(Args... args) const;

    private:
        connections_t connections;
        mutable iteration_tokens_t iteration_tokens;
    };

    // connection impl
    template <typename... Args>
    signal<void(Args...)>::connection::connection() noexcept
    : sig(nullptr), slot() {}

    template <typename... Args>
    signal<void(Args...)>::connection::connection(signal* sig, std::function<void(Args...)> slot) noexcept
            : sig(sig), slot(std::move(slot)) {
        sig->connections.push_back(*this);
    }

    template <typename... Args>
    signal<void(Args...)>::connection::connection(signal<void(Args...)>::connection&& rhs) noexcept
    : sig(rhs.sig), slot(std::move(rhs.slot)) {
        if (rhs.is_linked()) {
            replace_iteration_tokens_with_this(rhs);
        }
    }

    template <typename... Args>
    typename signals::signal<void(Args...)>::connection&
    signal<void(Args...)>::connection::operator=(signals::signal<void(Args...)>::connection&& rhs) noexcept {
        if (this != &rhs) {
            disconnect();
            sig = rhs.sig;
            slot = std::move(rhs.slot);
            replace_iteration_tokens_with_this(rhs);
        }
        return *this;
    }

    template <typename... Args>
    signal<void(Args...)>::connection::~connection() noexcept {
        disconnect();
    }

    template <typename... Args>
    void signal<void(Args...)>::connection::disconnect() noexcept {
        move_forward_iteration_tokens();
        if (list_auto_unlink_hook::is_linked()) {
            list_auto_unlink_hook::unlink();
            slot = {};
        }
    }

    template <typename... Args>
    void signal<void(Args...)>::connection::move_forward_iteration_tokens() noexcept {
        if (list_auto_unlink_hook::is_linked()) {
            for (auto &token : sig->iteration_tokens) {
                if (token.current != sig->connections.end() && &(*(token.current)) == this) {
                    ++token.current;
                }
            }
        }
    }

    template <typename... Args>
    void signal<void(Args...)>::connection::replace_iteration_tokens_with_this(connection& rhs) noexcept {
        assert(rhs.is_linked());
        swap_nodes(rhs);
        for (auto &token : rhs.sig->iteration_tokens) {
            if (token.current != rhs.sig->connections.end() && &(*(token.current)) == &rhs) {
                token.current = rhs.sig->connections.iterator_to(*this);
            }
        }
    }
    // connection impl end

    // signal impl
    template <typename... Args>
    typename signals::signal<void(Args...)>::connection
    signal<void(Args...)>::connect(slot_t slot) noexcept {
        return signals::signal<void(Args...)>::connection(this, std::move(slot));
    }

    // signal emit
    template <typename... Args>
    void signal<void(Args...)>::operator()(Args... args) const {
        iteration_token token(connections.begin());
        iteration_tokens.push_front(token);
        try {
            while (token.current != connections.end()) {
                auto copy = token.current;
                assert(token.current != connections.end());
                ++token.current;
                (copy->slot)(args...);
                if (token.is_signal_deleted) {
                    return;
                }
            }
        } catch (...) {
            iteration_tokens.pop_front();
            throw;
        }
        iteration_tokens.pop_front();
    }

    template <typename... Args>
    signal<void(Args...)>::~signal() noexcept {
        for (auto &token : iteration_tokens) {
            token.is_signal_deleted = true;
        }
    }
    // signal impl end
}
