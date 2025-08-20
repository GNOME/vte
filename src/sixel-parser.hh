/*
 * Copyright © 2018, 2020 Christian Persch
 *
 * This library is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library.  If not, see <https://www.gnu.org/licenses/>.
 */

#pragma once

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <initializer_list>
#include <type_traits>
#include <utility>

#include "cxx-utils.hh"
#include "parser-arg.hh"

#define VTE_SIXEL_PARSER_ARG_MAX (8)

namespace vte::sixel {

class Parser;

enum class Command : uint8_t {
        NONE = 0x20,
        DECGRI = 0x21, // DEC Graphics Repeat Introducer
        DECGRA = 0x22, // DEC Set Raster Attributes
        DECGCI = 0x23, // DEC Graphics Color Introducer
        DECGCR = 0x24, // DEC Graphics Carriage Return
        DECGCH = 0x2b, // DEC Graphics Cursor Home
        DECGNL = 0x2d, // DEC Graphics Next Line
        RESERVED_2_05 = 0x25,
        RESERVED_2_06 = 0x26,
        RESERVED_2_07 = 0x27,
        RESERVED_2_08 = 0x28,
        RESERVED_2_09 = 0x29,
        RESERVED_2_10 = 0x2a,
        RESERVED_2_12 = 0x2c,
        RESERVED_2_14 = 0x2e,
        RESERVED_2_15 = 0x2f,
        RESERVED_3_12 = 0x3c,
        RESERVED_3_13 = 0x3d,
        RESERVED_3_14 = 0x3e,
};

class Sequence {
protected:
        friend class Parser;

        unsigned m_command{(unsigned)Command::NONE};
        unsigned m_n_args{0};
        vte_seq_arg_t m_args[VTE_SIXEL_PARSER_ARG_MAX]{0, 0, 0, 0, 0, 0 ,0, 0};

        constexpr auto capacity() const noexcept
        {
                return sizeof(m_args) / sizeof(m_args[0]);
        }

public:

        constexpr Sequence() noexcept = default;

        Sequence(Command cmd,
                 std::initializer_list<int> params = {}) noexcept
                : m_command(std::to_underlying(cmd))
        {
                assert(params.size() <= capacity());
                for (auto p : params)
                        m_args[m_n_args++] = vte_seq_arg_init(std::min(p, 0xffff));
        }

        ~Sequence() = default;

        Sequence(Sequence const&) noexcept = default;
        Sequence(Sequence&&) noexcept = default;

        Sequence& operator=(Sequence const&) noexcept = default;
        Sequence& operator=(Sequence&&) noexcept = default;

        constexpr bool
        operator==(Sequence const& rhs) const noexcept
        {
                return command() == rhs.command() &&
                        size() == rhs.size() &&
                        std::memcmp(m_args, rhs.m_args, m_n_args * sizeof(m_args[0])) == 0;
        }

        /* command:
         *
         * Returns: the command the sequence codes for.
         */
        inline constexpr Command command() const noexcept
        {
                return Command(m_command);
        }

        /* size:
         *
         * Returns: the number of parameters
         */
        inline constexpr unsigned int size() const noexcept
        {
                return m_n_args;
        }

        /* param_default:
         * @idx:
         *
         * Returns: whether the parameter at @idx has default value
         */
        inline constexpr bool param_default(unsigned int idx) const noexcept
        {
                return __builtin_expect(idx < size(), 1) ? vte_seq_arg_default(m_args[idx]) : true;
        }

        /* param:
         * @idx:
         * @default_v: the value to use for default parameters
         *
         * Returns: the value of the parameter at index @idx, or @default_v if
         *   the parameter at this index has default value, or the index
         *   is out of bounds
         */
        inline constexpr int param(unsigned int idx,
                                   int default_v = -1) const noexcept
        {
                return __builtin_expect(idx < size(), 1) ? vte_seq_arg_value(m_args[idx], default_v) : default_v;
        }

        /* param:
         * @idx:
         * @default_v: the value to use for default parameters
         * @min_v: the minimum value
         * @max_v: the maximum value
         *
         * Returns: the value of the parameter at index @idx, or @default_v if
         *   the parameter at this index has default value, or the index
         *   is out of bounds. The returned value is clamped to the
         *   range @min_v..@max_v (or returns min_v, if min_v > max_v).
         */
        inline constexpr int param(unsigned int idx,
                                   int default_v,
                                   int min_v,
                                   int max_v) const noexcept
        {
                auto const v = param(idx, default_v);
                // not using std::clamp() since it's not guaranteed that min_v <= max_v
                return std::max(std::min(v, max_v), min_v);
        }

}; // class Sequence

/* SIXEL parser.
 *
 * Known differences to the DEC terminal SIXEL parser:
 *
 * * Input bytes with the high bit set are ignored, and not processed as if masked
 *   with ~0x80; except for C1 controls in Mode::EIGHTBIT mode which will abort parsing
 *
 * * Supports UTF-8 C1 controls. C1 ST will finish parsing; all other C1 controls
 *   will abort parsing (in Mode::UTF8)
 *
 * * All C0 controls (except CAN, ESC, SUB) and not just the format effector controls
 *  (HT, BS, LF, VT, FF, CR) are ignored, not executed as if received before the DCS start
 *
 * * 3/10 ':' is reserved for future use as subparameter separator analogous to
 *   the main parser; any parameter sequences including ':' will be ignored.
 *
 * * When the number of parameter exceeds the maximum (16), DEC executes the function
 *   with these parameters, ignoring the excessive parameters; vte ignores the
 *   whole function instead.
 */

class Parser {
public:
        enum class Mode {
                UTF8,     /* UTF-8          */
                EIGHTBIT, /* ECMA-35, 8 bit */
                SEVENBIT, /* ECMA-35, 7 bit */
        };

        enum class Status {
                CONTINUE = 0,
                COMPLETE,
                ABORT,
                ABORT_REWIND_ONE,
                ABORT_REWIND_TWO,
        };

        Parser() = default;
        ~Parser() = default;

        Parser(Mode mode) :
                m_mode{mode}
        {
        }

private:
        Parser(Parser const&) = delete;
        Parser(Parser&&) = delete;

        Parser& operator=(Parser const&) = delete;
        Parser& operator=(Parser&) = delete;

        enum class State {
                GROUND,  /* initial state and ground */
                PARAMS,  /* have command, now parsing parameters */
                IGNORE,  /* ignore until next command */
                ESC,     /* have seen ESC, waiting for backslash */
                UTF8_C2, /* have seen 0xC2, waiting for second UTF-8 byte */
        };

        Mode m_mode{Mode::UTF8};
        State m_state{State::GROUND};
        Sequence m_seq{};

        [[gnu::always_inline]]
        void
        params_clear() noexcept
        {
                /* The (m_n_args+1)th parameter may have been started but not
                 * finialised, so it needs cleaning too. All further params
                 * have not been touched, so need not be cleaned.
                 */
                unsigned int n_args = G_UNLIKELY(m_seq.m_n_args >= VTE_SIXEL_PARSER_ARG_MAX)
                        ? VTE_SIXEL_PARSER_ARG_MAX
                        : m_seq.m_n_args + 1;
                memset(m_seq.m_args, 0, n_args * sizeof(m_seq.m_args[0]));
#ifdef PARSER_EXTRA_CLEAN
                /* Assert that the assumed-clean params are actually clean. */
                for (auto n = n_args; n < VTE_SIXEL_PARSER_ARG_MAX; ++n)
                        vte_assert_cmpuint(m_seq.m_args[n], ==, VTE_SEQ_ARG_INIT_DEFAULT);
#endif

                m_seq.m_n_args = 0;
        }

        [[gnu::always_inline]]
        void
        params_overflow() noexcept
        {
                /* An overflow of the parameter number occurs when
                 * m_n_arg == VTE_SIXEL_PARSER_ARG_MAX, and either an 0…9
                 * is encountered, starting the next param, or an
                 * explicit ':' or ';' terminating a (defaulted) (sub)param,
                 * or when the next command or sixel data character occurs
                 * after a defaulted (sub)param.
                 *
                 * Transition to IGNORE to ignore the whole sequence.
                 */
                transition(0, State::IGNORE);
        }

        [[gnu::always_inline]]
        void
        params_finish() noexcept
        {
                if (G_LIKELY(m_seq.m_n_args < VTE_SIXEL_PARSER_ARG_MAX)) {
                        if (m_seq.m_n_args > 0 ||
                            vte_seq_arg_started(m_seq.m_args[m_seq.m_n_args])) {
                                vte_seq_arg_finish(&m_seq.m_args[m_seq.m_n_args], false);
                                ++m_seq.m_n_args;
                        }
                }
        }

        [[gnu::always_inline]]
        Status
        param_finish(uint8_t raw) noexcept
        {
                if (G_LIKELY(m_seq.m_n_args < VTE_SIXEL_PARSER_ARG_MAX - 1)) {
                        vte_seq_arg_finish(&m_seq.m_args[m_seq.m_n_args], false);
                        ++m_seq.m_n_args;
                } else
                        params_overflow();

                return Status::CONTINUE;
        }

        [[gnu::always_inline]]
        Status
        param(uint8_t raw) noexcept
        {
                if (G_LIKELY(m_seq.m_n_args < VTE_SIXEL_PARSER_ARG_MAX))
                        vte_seq_arg_push(&m_seq.m_args[m_seq.m_n_args], raw);
                else
                        params_overflow();

                return Status::CONTINUE;
        }

        template<class D, class = std::void_t<>>
        struct has_SIXEL_CMD_member : std::false_type { };

        template<class D>
        struct has_SIXEL_CMD_member<D, std::void_t<decltype(&D::SIXEL_CMD)>> : std::true_type { };

        template<class D>
        [[gnu::always_inline]]
        std::enable_if_t<has_SIXEL_CMD_member<D>::value>
        dispatch(uint8_t raw,
                 D& delegate) noexcept
        {
                params_finish();
                delegate.SIXEL_CMD(m_seq);
        }

        template<class D>
        [[gnu::always_inline]]
        std::enable_if_t<!has_SIXEL_CMD_member<D>::value>
        dispatch(uint8_t raw,
                 D& delegate) noexcept
        {
                params_finish();
                switch (m_seq.command()) {
                case Command::DECGRI: return delegate.DECGRI(m_seq);
                case Command::DECGRA: return delegate.DECGRA(m_seq);
                case Command::DECGCI: return delegate.DECGCI(m_seq);
                case Command::DECGCR: return delegate.DECGCR(m_seq);
                case Command::DECGCH: return delegate.DECGCH(m_seq);
                case Command::DECGNL: return delegate.DECGNL(m_seq);
                case Command::NONE:   return;
                case Command::RESERVED_2_05:
                case Command::RESERVED_2_06:
                case Command::RESERVED_2_07:
                case Command::RESERVED_2_08:
                case Command::RESERVED_2_09:
                case Command::RESERVED_2_10:
                case Command::RESERVED_2_12:
                case Command::RESERVED_2_14:
                case Command::RESERVED_2_15:
                case Command::RESERVED_3_12:
                case Command::RESERVED_3_13:
                case Command::RESERVED_3_14: return delegate.SIXEL_NOP(m_seq);
                default: __builtin_unreachable(); return;
                }
        }

        template<class D>
        [[gnu::always_inline]]
        Status
        data(uint8_t sixel,
             D& delegate) noexcept
        {
                delegate.SIXEL(sixel);
                return Status::CONTINUE;
        }

        [[gnu::always_inline]]
        Status
        transition(uint8_t raw,
                   State state) noexcept
        {
                m_state = state;
                return Status::CONTINUE;

        }

        [[gnu::always_inline]]
        Status
        abort(uint8_t raw,
              Status result) noexcept
        {
                transition(raw, State::GROUND);
                return result;
        }

        template<class D>
        [[gnu::always_inline]]
        Status
        complete(uint8_t raw,
                 D& delegate) noexcept
        {
                transition(raw, State::GROUND);
                delegate.SIXEL_ST(raw);
                return Status::COMPLETE;
        }

        [[gnu::always_inline]]
        Status
        consume(uint8_t raw) noexcept
        {
                params_clear();
                m_seq.m_command = raw;
                return transition(raw, State::PARAMS);
        }

        [[gnu::always_inline]]
        Status
        nop(uint8_t raw) noexcept
        {
                return Status::CONTINUE;
        }

public:

        template<class D>
        Status
        feed(uint8_t raw,
             D& delegate) noexcept
        {
                // Refer to Table 2-2 in DECPPLV2 for information how to handle C0 and C1
                // controls, DEL, and GR data (in 8-bit mode).
                switch (m_state) {
                case State::PARAMS:
                        switch (raw) {
                        case 0x00 ... 0x17:
                        case 0x19:
                        case 0x1c ... 0x1f: /* C0 \ { CAN, SUB, ESC } */
                                /* FIXMEchpe: maybe only do this for the format effector controls?,
                                 * and let GROUND handle everything else C0?
                                 */
                                return nop(raw);
                        case 0x30 ... 0x39: /* '0' ... '9' */
                                return param(raw);
                        case 0x3a: /* ':' */
                                // Reserved for subparams; just ignore the whole sequence.
                                return transition(raw, State::IGNORE);
                        case 0x3b: /* ';' */
                                return param_finish(raw);
                        case 0x7f: /* DEL */
                        case 0xa0 ... 0xc1:
                        case 0xc3 ... 0xff:
                                return nop(raw);
                        case 0xc2: /* Start byte for UTF-8 C1 controls */
                                if (m_mode == Mode::EIGHTBIT)
                                        return nop(raw);

                                [[fallthrough]];
                        case 0x80 ... 0x9f:
                                if (m_mode == Mode::SEVENBIT)
                                        return nop(raw);

                                [[fallthrough]];
                        case 0x18: /* CAN */
                        case 0x1b: /* ESC */
                        case 0x20 ... 0x2f:
                        case 0x3c ... 0x7e:
                                // Dispatch the current command and continue parsing
                                dispatch(raw, delegate);
                                [[fallthrough]];
                        case 0x1a: /* SUB */
                                /* The question is whether SUB should only act like '?' or
                                 * also dispatch the current sequence. I interpret the DEC
                                 * docs as indicating it aborts the sequence without dispatching
                                 * it and only inserts the '?'.
                                 */
                                transition(raw, State::GROUND);;
                        }

                        [[fallthrough]];
                case State::GROUND:
                ground:
                        switch (raw) {
                        case 0x00 ... 0x17:
                        case 0x19:
                        case 0x1c ... 0x1f: /* C0 \ { CAN, SUB, ESC } */
                                // According to DECPPLV2, the format effector controls
                                // (HT, BS, LF, VT, FF, CR) should be executed as if
                                // received before the DECSIXEL DCS, and then processing
                                // to continue for the control string, and the other C0
                                // controls should be ignored.
                                // VTE just ignores all C0 controls except ESC, CAN, SUB
                                return nop(raw);
                        case 0x18: /* CAN */
                                return abort(raw, Status::ABORT_REWIND_ONE);
                        case 0x1b: /* ESC */
                                return transition(raw, State::ESC);
                        case 0x20: /* SP */
                                return nop(raw);
                        case 0x21 ... 0x2f:
                        case 0x3c ... 0x3e:
                                return consume(raw);
                        case 0x30 ... 0x3b: /* { '0' .. '9', ':', ';' } */
                                // Parameters, but we don't have a command yet.
                                // Ignore the whole sequence.
                                return transition(raw, State::IGNORE);
                        case 0x1a: /* SUB */
                                // Same as 3/15 '?' according to DECPPLV2
                                raw = 0x3fu;
                                [[fallthrough]];
                        case 0x3f ... 0x7e: /* { '?' .. '~' } */
                                // SIXEL data
                                return data(raw - 0x3f, delegate);
                        case 0x7f: /* DEL */
                                // Ignore according to DECPPLV2
                                return nop(raw);
                        case 0xc2: /* Start byte for UTF-8 C1 controls */
                                if (m_mode == Mode::UTF8)
                                        return transition(raw, State::UTF8_C2);
                                return nop(raw);
                        case 0x9c: /* raw C1 ST */
                                if (m_mode == Mode::EIGHTBIT)
                                        return complete(raw, delegate);
                                [[fallthrough]];
                        case 0x80 ... 0x9b:
                        case 0x9d ... 0x9f: /* raw C1 \ { ST } */
                                // Abort and execute C1 control
                                if (m_mode == Mode::EIGHTBIT)
                                        return abort(raw, Status::ABORT_REWIND_ONE);
                                [[fallthrough]];
                        case 0xa0 ... 0xc1:
                        case 0xc3 ... 0xff: /* GR */
                                return nop(raw);

                        }
                        break;

                case State::IGNORE:
                        switch (raw) {
                        // FIXMEchpe do we need to nop() C0 constrols (except SUB, CAN, ESC) here?
                        case 0x30 ... 0x3b: /* { '0' .. '9', ':', ';' } */
                        case 0x7f: /* DEL */
                                return nop(raw);
                        case 0x00 ... 0x2f:
                        case 0x3c ... 0x7e:
                        case 0x80 ... 0xff:
                                transition(raw, State::GROUND);
                                goto ground;
                        }
                        break;

                case State::ESC:
                        switch (raw) {
                        case 0x5c: /* '\' */
                                return complete(raw, delegate);
                        case 0x7f: /* DEL */
                                // FIXMEchpe is this correct? check with main parser / spec / DEC
                                return nop(raw);
                        case 0x00 ... 0x5b:
                        case 0x5d ... 0x7e:
                        case 0x80 ... 0xff:
                                /* Abort and let the outer parser handle the ESC again */
                                return abort(raw, Status::ABORT_REWIND_TWO);
                        }
                        break;

                case State::UTF8_C2:
                        switch (raw) {
                        case 0x1b: /* ESC */
                                return transition(raw, State::ESC);
                        case 0x80 ... 0x9b:
                        case 0x9d ... 0x9f: /* C1 \ { ST } */
                                /* Abort and let the outer parser handle the C1 control again */
                                return abort(raw, Status::ABORT_REWIND_TWO);
                        case 0x9c: /* ST */
                                return complete(raw, delegate);
                        case 0xc2:
                                return transition(raw, State::UTF8_C2);
                        case 0x00 ... 0x1a:
                        case 0x1c ... 0x7f: /* including DEL */
                        case 0xa0 ... 0xc1:
                        case 0xc3 ... 0xff:
                                transition(raw, State::GROUND);
                                goto ground;
                        }
                        break;
                default:
                        break;
                }
                __builtin_unreachable();
                return Status::CONTINUE;
        }

        template<class D>
        Status
        flush(D& delegate) noexcept
        {
                switch (m_state) {
                case State::PARAMS:
                        dispatch(0, delegate);
                        [[fallthrough]];
                case State::GROUND:
                case State::IGNORE:
                        return abort(0, Status::ABORT);
                default:
                        __builtin_unreachable();
                        [[fallthrough]];
                case State::ESC:
                case State::UTF8_C2:
                        return abort(0, Status::ABORT_REWIND_ONE);
                }
        }

        void
        reset() noexcept
        {
                transition(0, State::GROUND);
        }

        void
        set_mode(Mode mode) noexcept
        {
                reset();
                m_mode = mode;
        }

        constexpr auto const& sequence() const noexcept { return m_seq; }

        enum class ParseStatus {
                CONTINUE,
                COMPLETE,
                ABORT
        };

        template<class D>
        std::pair<ParseStatus, uint8_t const*>
        parse(uint8_t const* const bufstart,
              uint8_t const* const bufend,
              bool eos,
              D& delegate) noexcept
        {
                for (auto sptr = bufstart; sptr < bufend; ) {
                        switch (feed(*(sptr++), delegate)) {
                        case vte::sixel::Parser::Status::CONTINUE:
                                break;

                        case vte::sixel::Parser::Status::COMPLETE:
                                return {ParseStatus::COMPLETE, sptr};

                        case vte::sixel::Parser::Status::ABORT_REWIND_TWO:
                                --sptr;
                                [[fallthrough]];
                        case vte::sixel::Parser::Status::ABORT_REWIND_ONE:
                                --sptr;
                                [[fallthrough]];
                        case vte::sixel::Parser::Status::ABORT:
                                return {ParseStatus::ABORT, sptr};
                        }
                }

                if (eos) {
                        auto sptr = bufend;
                        switch (flush(delegate)) {
                        case vte::sixel::Parser::Status::CONTINUE:
                                break;

                        case vte::sixel::Parser::Status::COMPLETE:
                                return {ParseStatus::COMPLETE, sptr};

                        case vte::sixel::Parser::Status::ABORT_REWIND_TWO:
                                --sptr;
                                [[fallthrough]];
                        case vte::sixel::Parser::Status::ABORT_REWIND_ONE:
                                --sptr;
                                [[fallthrough]];
                        case vte::sixel::Parser::Status::ABORT:
                                return {ParseStatus::ABORT, sptr};
                        }
                }

                return {ParseStatus::CONTINUE, bufend};
        }

}; // class Parser

} // namespace vte::sixel
