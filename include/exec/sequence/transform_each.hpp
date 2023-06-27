/*
 * Copyright (c) 2023 Maikel Nadolski
 * Copyright (c) 2023 NVIDIA Corporation
 *
 * Licensed under the Apache License Version 2.0 with LLVM Exceptions
 * (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 *
 *   https://llvm.org/LICENSE.txt
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#pragma once

#include "../sequence_senders.hpp"

namespace exec {
  namespace __transform {
    using namespace stdexec;

    template <class _Receiver, class _Adaptor>
    struct __operation_base {
      _Receiver __receiver_;
      _Adaptor __adaptor_;
    };

    template <class _ReceiverId, class _Adaptor>
    struct __receiver {
      using _Receiver = stdexec::__t<_ReceiverId>;

      struct __t {
        using is_receiver = void;
        __operation_base<_Receiver, _Adaptor>* __op_;

        template <same_as<set_next_t> _SetNext, same_as<__t> _Self, class _Item>
          requires __callable<_Adaptor&, _Item>
                && __callable<exec::set_next_t, _Receiver&, __call_result_t<_Adaptor&, _Item>>
        friend auto tag_invoke(_SetNext, _Self& __self, _Item&& __item) noexcept(
          __nothrow_callable<_SetNext, _Receiver&, __call_result_t<_Adaptor&, _Item>> //
            && __nothrow_callable<_Adaptor&, _Item>)
          -> __next_sender_of_t<_Receiver, __call_result_t<_Adaptor&, _Item>> {
          return exec::set_next(
            __self.__op_->__receiver_, __self.__op_->__adaptor_(static_cast<_Item&&>(__item)));
        }

        template <same_as<set_value_t> _SetValue, same_as<__t> _Self>
        friend void tag_invoke(_SetValue, _Self&& __self) noexcept {
          stdexec::set_value(static_cast<_Receiver&&>(__self.__op_->__receiver_));
        }

        template <same_as<set_stopped_t> _SetStopped, same_as<__t> _Self>
          requires __callable<_SetStopped, _Receiver&&>
        friend void tag_invoke(_SetStopped, _Self&& __self) noexcept {
          stdexec::set_stopped(static_cast<_Receiver&&>(__self.__op_->__receiver_));
        }

        template <same_as<set_error_t> _SetError, same_as<__t> _Self, class _Error>
          requires __callable<_SetError, _Receiver&&, _Error>
        friend void tag_invoke(_SetError, _Self&& __self, _Error&& __error) noexcept {
          stdexec::set_error(
            static_cast<_Receiver&&>(__self.__op_->__receiver_), static_cast<_Error&&>(__error));
        }

        template <same_as<get_env_t> _GetEnv, __decays_to<__t> _Self>
        friend env_of_t<_Receiver> tag_invoke(_GetEnv, _Self&& __self) noexcept {
          return stdexec::get_env(__self.__op_->__receiver_);
        }
      };
    };

    template <class _Sender, class _ReceiverId, class _Adaptor>
    struct __operation {
      using _Receiver = stdexec::__t<_ReceiverId>;

      struct __t : __operation_base<_Receiver, _Adaptor> {
        subscribe_result_t<_Sender, stdexec::__t<__receiver<_ReceiverId, _Adaptor>>> __op_;

        __t(_Sender&& __sndr, _Receiver __rcvr, _Adaptor __adaptor)
          : __operation_base<
            _Receiver,
            _Adaptor>{static_cast<_Receiver&&>(__rcvr), static_cast<_Adaptor&&>(__adaptor)}
          , __op_{exec::subscribe(
              static_cast<_Sender&&>(__sndr),
              stdexec::__t<__receiver<_Receiver, _Adaptor>>{this})} {
        }

        friend void tag_invoke(start_t, __t& __self) noexcept {
          stdexec::start(__self.__op_);
        }
      };
    };

    template <class _SenderId, class _Adaptor>
    struct __sender {
      using _Sender = stdexec::__t<_SenderId>;

      struct __t {
        using is_sender = sequence_tag;
        using __id = __sender;

        _Sender __sender_;
        _Adaptor __adaptor_;

        template <class _Self, class _Env>
        using __some_sender = __copy_cvref_t<
          _Self,
          __sequence_sndr::__unspecified_sender_of<
            completion_signatures_of_t<__copy_cvref_t<_Self, _Sender>, _Env>>>;

        template <class _Self, class _Env>
        using completion_sigs_t = completion_signatures_of_t<
          __call_result_t<_Adaptor&, __some_sender<_Self, _Sender>>,
          _Env>;

        template <__decays_to<__t> _Self, receiver _Receiver>
        requires sequence_receiver_of<_Receiver, completion_sigs_t<_Self, env_of_t<_Receiver>>>
              && sequence_sender_to<
                   __copy_cvref_t<_Self, _Sender>,
                   stdexec::__t<__receiver<stdexec::__id<_Receiver>, _Adaptor>>>
        friend auto tag_invoke(subscribe_t, _Self&& __self, _Receiver __rcvr) -> stdexec::__t<
          __operation<__copy_cvref_t<_Self, _Sender>, stdexec::__id<_Receiver>, _Adaptor>> {
          return {
            static_cast<_Self&&>(__self).__sender_,
            static_cast<_Receiver&&>(__rcvr),
            static_cast<_Self&&>(__self).__adaptor_};
        }

        template <__decays_to<__t> _Self, class _Env>
        friend auto tag_invoke(get_completion_signatures_t, _Self&&, _Env&&)
          -> completion_sigs_t<_Self, _Env>;

        friend env_of_t<_Sender> tag_invoke(get_env_t, const __t& __self) noexcept {
          return stdexec::get_env(__self.__sender_);
        }
      };
    };

    struct transform_each_t {
      template <sender _Sender, class _Adaptor>
      auto operator()(_Sender&& __sndr, _Adaptor&& __adaptor) const
        noexcept(__nothrow_decay_copyable<_Sender> //
                   && __nothrow_decay_copyable<_Adaptor>)
          -> __t<__sender<__id<__decay_t<_Sender>>, __decay_t<_Adaptor>>> {
        return {static_cast<_Sender&&>(__sndr), static_cast<_Adaptor&&>(__adaptor)};
      }

      template <class _Adaptor>
      constexpr auto operator()(_Adaptor __adaptor) const noexcept
        -> __binder_back<transform_each_t, _Adaptor> {
        return {{}, {}, {static_cast<_Adaptor&&>(__adaptor)}};
      }
    };
  }

  using __transform::transform_each_t;
  inline constexpr transform_each_t transform_each{};
}