#pragma once

// This file contains several pre-defined matrix interconnections establishing which nodes in a
// N-quorum establish connections to each other.  The patterns here were determined by the
// utils/generate-quorum-matrix.py script.

#include <array>

namespace arqnet {

class quorum_conn_iterator {
private:
    friend class quorum_outgoing_conns;
    friend class quorum_incoming_conns;
    const bool * const flags;
    int i;
    const int N;
    const int step;

    quorum_conn_iterator(const bool *flags_, int N_, bool outgoing_ = true) : flags{flags_}, i{0}, N{N_}, step{outgoing_ ? 1 : N} {
        if (flags == nullptr && N != 0)
            throw std::domain_error("Invalid/unsupported quorum size (" + std::to_string(N) + ")");
        if (!flags[0] && N > 0) ++*this;
    }

public:
    using iterator_category = std::forward_iterator_tag;
    using value_type = int;
    using difference_type = int;
    using pointer_type = const int *;
    using reference = const int &;

    constexpr quorum_conn_iterator() : flags{nullptr}, i{0}, N{0}, step{1} {}

    reference operator*() const { return i; }

    bool operator==(const quorum_conn_iterator &it) const {
        return (i == N && it.i == it.N) || (flags == it.flags && i == it.i);
    }

    bool operator!=(const quorum_conn_iterator &it) const { return !(*this == it); }

    quorum_conn_iterator &operator++() {
        do { ++i; } while (i < N && !flags[i*step]);
        return *this;
    }

    quorum_conn_iterator operator++(int) {
        quorum_conn_iterator old{*this};
        ++*this;
        return old;
    }
};

template <int N> constexpr void requested_quorum_size_is_not_defined() { static_assert(N != N, "Requested quorum size is not implemented"); }

/// Base implementation for quorum matrices; instantiating this is an error; all supported sizes are
/// defined below.
template <int N> constexpr static std::array<bool, N*N> quorum_conn_matrix = requested_quorum_size_is_not_defined<N>();

template<> constexpr std::array<bool, 7*7> quorum_conn_matrix<7>{{
    0,1,0,1,0,1,0,
    0,0,1,1,0,0,1,
    1,0,0,1,1,0,0,
    0,0,0,0,1,1,1,
    1,0,0,0,0,1,0,
    0,1,0,0,0,0,1,
    0,0,1,0,1,0,0,
}};

template<> constexpr std::array<bool, 8*8> quorum_conn_matrix<8>{{
    0,1,1,1,0,1,0,0,
    0,0,1,1,1,0,0,0,
    0,0,0,1,0,1,1,0,
    0,0,0,0,1,0,1,1,
    1,0,1,0,0,0,0,1,
    0,1,0,0,0,0,1,0,
    1,0,0,0,1,0,0,1,
    1,1,0,0,0,1,0,0,
}};

template<> constexpr std::array<bool, 9*9> quorum_conn_matrix<9>{{
    0,1,1,0,0,1,0,0,0,
    0,0,1,0,0,1,0,1,0,
    0,0,0,1,1,0,0,0,1,
    0,0,0,0,1,0,1,1,0,
    1,1,0,0,0,0,1,0,0,
    0,0,1,1,0,0,1,0,0,
    1,0,0,0,0,0,0,1,1,
    1,0,0,0,1,0,0,0,1,
    0,1,0,1,0,1,0,0,0,
}};

template<> constexpr std::array<bool, 10*10> quorum_conn_matrix<10>{{
    0,1,0,0,0,1,0,0,0,1,
    0,0,1,0,0,0,1,1,0,0,
    0,0,0,1,0,0,0,0,1,1,
    0,0,0,0,1,1,1,0,0,0,
    1,0,0,0,0,0,0,1,1,0,
    0,1,1,0,0,0,1,0,0,0,
    1,0,0,0,1,0,0,1,0,0,
    0,0,1,1,0,0,0,0,1,0,
    1,1,0,0,0,0,0,0,0,1,
    0,0,0,1,1,1,0,0,0,0,
}};

template<> constexpr std::array<bool, 13*13> quorum_conn_matrix<13>{{
    0,1,1,1,0,1,0,0,0,0,0,0,0,
    0,0,1,0,0,0,0,1,0,0,0,0,1,
    0,0,0,1,0,0,1,1,0,0,0,0,0,
    0,0,0,0,1,1,0,0,0,1,0,0,0,
    1,0,0,0,0,0,0,0,0,0,1,1,0,
    0,0,0,0,0,0,1,0,1,0,1,0,0,
    0,0,0,0,1,0,0,1,0,1,0,0,0,
    1,0,0,0,1,0,0,0,1,0,0,0,0,
    0,1,0,1,0,0,0,0,0,1,0,0,0,
    0,0,0,0,0,1,0,0,0,0,0,1,1,
    1,0,1,0,0,0,0,0,0,0,0,1,0,
    0,1,0,0,0,0,0,0,1,0,0,0,1,
    0,0,1,0,0,0,1,0,0,0,1,0,0,
}};

template<> constexpr std::array<bool, 14*14> quorum_conn_matrix<14>{{
    0,1,0,0,0,1,0,0,0,0,0,0,0,1,
    0,0,1,0,1,0,0,0,1,0,0,0,0,0,
    0,0,0,1,0,0,0,1,1,0,0,0,0,0,
    0,0,0,0,1,0,1,0,0,1,0,0,0,0,
    1,0,0,0,0,1,1,0,0,0,0,0,0,0,
    0,0,0,0,0,0,1,1,0,0,1,0,0,0,
    0,1,0,0,0,0,0,1,0,0,1,1,0,0,
    0,0,0,0,0,0,0,0,1,1,0,0,1,0,
    0,0,0,1,0,0,0,0,0,1,0,0,0,1,
    0,0,0,0,0,1,0,0,0,0,0,1,1,0,
    1,0,1,0,0,0,0,0,0,0,0,1,0,0,
    0,1,0,0,1,0,0,0,0,0,0,0,1,0,
    1,0,0,0,0,0,0,0,0,0,0,0,0,1,
    0,0,1,1,0,0,0,0,0,0,1,0,0,0,
}};

template<> constexpr std::array<bool, 15*15> quorum_conn_matrix<15>{{
    0,1,1,0,0,1,0,0,0,0,0,0,0,0,0,
    0,0,1,0,0,0,1,0,1,1,0,0,0,0,0,
    0,0,0,1,0,0,1,1,0,0,0,1,0,0,0,
    0,0,0,0,1,0,1,0,0,1,0,0,0,0,0,
    1,0,0,0,0,0,0,0,0,0,0,0,1,0,1,
    0,1,0,0,0,0,1,0,0,0,1,0,0,0,0,
    0,0,0,0,1,0,0,1,0,0,1,0,0,0,0,
    0,0,0,0,0,0,0,0,1,1,0,0,1,0,0,
    1,0,0,0,0,0,0,0,0,1,0,1,0,0,0,
    0,0,0,0,0,1,0,0,0,0,0,0,0,1,1,
    0,0,1,0,0,0,0,0,0,0,0,1,0,1,0,
    0,0,0,1,0,0,0,1,0,0,0,0,1,0,0,
    1,0,0,0,0,1,0,0,0,0,0,0,0,1,0,
    0,1,0,0,1,0,0,0,0,0,0,0,0,0,1,
    0,0,0,1,0,0,0,0,1,0,1,0,0,0,0,
}};

template<> constexpr std::array<bool, 16*16> quorum_conn_matrix<16>{{
    0,1,1,0,0,0,0,0,0,0,0,0,1,0,0,1,
    0,0,1,0,0,1,0,0,0,0,0,0,1,1,0,0,
    0,0,0,1,0,0,0,0,1,0,1,0,0,0,0,1,
    0,0,0,0,1,0,0,0,1,0,0,1,0,0,0,0,
    1,0,0,0,0,0,0,0,0,0,0,1,0,0,0,1,
    0,0,1,0,0,0,1,0,0,0,0,0,0,0,1,0,
    1,0,0,1,0,0,0,1,0,0,0,0,0,0,0,0,
    1,0,0,0,0,0,0,0,1,1,0,0,0,0,0,0,
    0,1,0,0,0,0,0,0,0,1,0,0,0,1,0,0,
    0,1,0,0,1,1,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,1,0,0,0,1,1,0,0,0,
    0,0,0,0,0,0,1,0,0,1,0,0,1,0,0,0,
    0,0,0,0,0,1,1,0,0,0,0,0,0,1,0,0,
    0,0,0,1,0,0,0,0,0,0,0,0,0,0,1,1,
    0,0,0,0,1,0,0,1,0,0,1,0,0,0,0,0,
    0,0,0,0,0,1,0,0,0,0,1,0,0,0,1,0,
}};

template<> constexpr std::array<bool, 17*17> quorum_conn_matrix<17>{{
    0,1,0,0,0,0,1,1,0,0,0,0,0,0,0,1,0,
    0,0,1,0,0,1,0,1,0,0,0,0,0,0,0,0,0,
    0,0,0,1,1,1,0,0,1,0,0,0,0,0,0,0,0,
    0,0,0,0,1,0,1,0,0,0,0,0,1,0,0,0,0,
    1,1,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,
    0,0,0,0,0,0,1,0,0,0,1,1,0,0,0,1,0,
    0,0,0,0,0,0,0,1,0,0,0,0,0,0,1,1,0,
    0,0,0,0,0,0,0,0,1,0,1,1,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,1,0,1,0,1,0,0,1,
    0,0,0,0,0,1,1,0,0,0,0,0,0,1,0,0,0,
    1,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,1,
    0,0,0,1,0,0,0,0,0,1,0,0,1,0,0,0,0,
    1,0,0,0,0,0,0,0,1,0,0,0,0,1,0,0,0,
    0,1,0,0,1,0,0,0,0,0,0,0,0,0,1,0,0,
    0,0,0,1,1,0,0,0,0,0,1,0,0,0,0,0,0,
    0,0,1,0,0,0,0,0,0,0,0,0,1,0,0,0,1,
    0,0,1,0,0,0,0,0,0,0,0,0,0,1,1,0,0,
}};

template<> constexpr std::array<bool, 18*18> quorum_conn_matrix<18>{{
    0,1,0,1,0,1,0,0,1,0,0,0,0,0,0,0,0,0,
    0,0,1,0,0,0,0,0,0,1,0,0,0,1,0,1,0,0,
    0,0,0,1,0,0,0,1,0,0,0,0,0,0,0,1,1,0,
    0,1,0,0,1,0,1,0,0,1,0,0,0,0,0,0,0,0,
    1,0,0,0,0,0,1,0,0,0,0,0,0,0,1,0,0,0,
    0,0,0,0,0,0,1,1,0,0,1,0,0,0,1,0,0,0,
    0,1,0,0,0,0,0,1,0,1,0,0,1,0,0,0,0,0,
    0,0,0,0,0,0,0,0,1,0,0,1,1,0,0,0,1,0,
    0,0,1,0,0,0,0,0,0,1,0,0,0,0,0,0,0,1,
    0,0,0,0,0,1,0,0,0,0,0,0,0,1,0,0,0,1,
    0,0,1,1,0,0,0,0,0,0,0,1,0,0,0,0,0,0,
    0,0,0,1,0,0,0,0,0,0,0,0,1,0,0,1,0,0,
    1,0,0,0,1,0,0,0,0,0,0,0,0,1,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,1,0,0,0,1,0,1,0,
    1,0,0,0,0,0,0,0,1,0,1,0,0,0,0,0,0,1,
    0,0,0,0,0,1,1,0,0,0,0,0,0,0,0,0,1,0,
    0,0,0,0,1,0,0,0,0,0,0,0,1,0,0,0,0,1,
    0,0,0,0,0,0,0,1,0,0,0,1,0,0,0,1,0,0,
}};

template<> constexpr std::array<bool, 19*19> quorum_conn_matrix<19>{{
    0,1,0,0,0,1,0,1,0,0,0,0,0,0,0,0,0,0,0,
    0,0,1,1,0,0,0,0,0,1,0,0,0,0,0,1,0,0,0,
    0,0,0,1,0,0,0,1,0,0,0,0,1,0,0,1,0,0,0,
    0,0,0,0,1,0,0,0,0,1,0,1,0,0,0,0,0,0,0,
    1,0,0,0,0,0,0,0,0,0,0,0,0,1,1,0,0,1,0,
    0,0,0,0,0,0,1,0,0,0,1,1,0,0,0,0,0,1,0,
    0,0,0,0,0,0,0,1,0,0,0,1,1,0,0,0,1,0,0,
    0,0,0,0,0,0,0,0,1,0,0,0,1,0,1,0,0,0,0,
    0,0,0,1,0,0,0,0,0,1,0,0,0,1,0,0,0,1,0,
    0,0,0,0,0,1,1,0,0,0,0,0,0,1,0,0,0,0,0,
    0,1,1,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,
    0,0,0,0,1,0,0,0,0,0,0,0,1,0,0,0,0,0,1,
    1,0,0,0,1,0,0,0,0,0,0,0,0,1,0,0,0,0,1,
    0,0,0,0,0,0,0,0,0,0,1,0,0,0,1,0,1,0,0,
    0,0,0,0,0,0,0,0,1,0,1,0,0,0,0,1,0,0,1,
    0,0,0,0,0,1,1,0,0,0,0,0,0,0,0,0,1,0,0,
    0,0,1,0,1,0,0,0,0,0,0,0,0,0,0,0,0,1,0,
    1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,
    0,0,0,1,0,0,0,0,1,0,0,0,0,0,0,1,0,0,0,
}};

template<> constexpr std::array<bool, 20*20> quorum_conn_matrix<20>{{
    0,1,0,0,0,1,0,0,1,0,0,0,0,0,0,1,0,0,0,0,
    0,0,1,0,0,0,0,0,0,0,0,0,0,1,0,1,0,0,0,0,
    0,0,0,1,0,0,0,1,0,0,0,0,0,0,0,1,0,0,0,1,
    0,0,0,0,1,1,0,0,0,1,1,0,0,0,0,0,0,0,0,0,
    1,0,0,0,0,0,1,0,0,0,0,0,1,0,1,0,0,0,0,0,
    0,0,0,0,0,0,1,0,0,0,1,1,0,0,1,0,0,0,0,0,
    0,0,0,0,0,0,0,1,0,0,0,1,0,1,0,0,1,0,0,0,
    0,0,0,0,1,0,0,0,1,0,0,0,1,0,0,0,0,0,1,0,
    0,0,0,1,0,1,0,0,0,1,0,0,0,0,0,0,0,1,0,0,
    0,1,0,0,1,1,0,0,0,0,0,0,0,1,0,0,0,0,0,0,
    0,0,1,0,0,0,0,0,0,0,0,1,0,0,0,0,0,1,0,0,
    1,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0,1,0,
    1,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,1,0,
    0,0,0,0,0,0,0,0,0,0,0,1,0,0,1,0,1,0,0,1,
    0,0,0,0,0,0,0,0,0,0,1,0,1,0,0,0,0,0,0,1,
    0,0,0,0,0,0,1,1,0,0,0,0,0,0,0,0,1,1,0,0,
    0,0,0,1,1,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,
    0,1,0,0,0,0,0,1,0,0,0,0,0,0,1,0,0,0,1,0,
    0,0,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,
    0,0,0,0,0,0,0,0,1,1,0,0,0,0,0,1,0,0,0,0,
}};

/// Returns the maximum possible outgoing connections for a single quorum of any supported size.
constexpr int max_outgoing_conns() { return 5; }

constexpr const bool *get_matrix(int N) {
    switch (N) {
        case 7: return &quorum_conn_matrix<7>[0];
        case 8: return &quorum_conn_matrix<8>[0];
        case 9: return &quorum_conn_matrix<9>[0];
        case 10: return &quorum_conn_matrix<10>[0];
        case 13: return &quorum_conn_matrix<13>[0];
        case 14: return &quorum_conn_matrix<14>[0];
        case 15: return &quorum_conn_matrix<15>[0];
        case 16: return &quorum_conn_matrix<16>[0];
        case 17: return &quorum_conn_matrix<17>[0];
        case 18: return &quorum_conn_matrix<18>[0];
        case 19: return &quorum_conn_matrix<19>[0];
        case 20: return &quorum_conn_matrix<20>[0];
        default: return nullptr;
    };
}

/// Iterable class that lets you iterate through peer indices that should have an outgoing
/// connection
class quorum_outgoing_conns {
    quorum_conn_iterator begin_;

public:
    /// Constructs an iterable object iterating over the peers of node `i` in a quorum of size `N`
    quorum_outgoing_conns(int i, int N) : begin_{get_matrix(N) + i*N, N} {}

    using iterator = quorum_conn_iterator;
    iterator begin() const { return begin_; }
    static constexpr iterator end() { return {}; }
};

class quorum_incoming_conns {
  quorum_conn_iterator begin_;

public:
  quorum_incoming_conns(int i, int N) : begin_{get_matrix(N) + i, N, false} {}

  using iterator = quorum_conn_iterator;
  iterator begin() const { return begin_; }
  static constexpr iterator end() { return {}; }
};

}
