#include <afina/coroutine/Engine.h>

#include <setjmp.h>
#include <stdio.h>
#include <cstring>
#include <cassert>
#include <iostream>

namespace Afina {
namespace Coroutine {

Engine::~Engine() {
    if (StackBottom) {
        delete[] std::get<0>(idle_ctx->Stack);
        delete idle_ctx;
    }

    for (auto coro = alive; coro != nullptr;) {
        auto tmp = coro;
        coro = coro->next;
        delete[] std::get<0>(tmp->Stack);
        delete tmp;
    }

    for (auto coro = blocked; coro != nullptr;) {
        auto tmp = coro;
        coro = coro->next;
        delete[] std::get<0>(tmp->Stack);
        delete tmp;
    }
}

void Engine::Store(context &ctx) {
    char begin_address;

    // this condition is necessary for architectures
    // where stack low address might be greater than high address
    if (&begin_address > ctx.Low) {
        ctx.High = &begin_address;
    } else {
        ctx.Low = &begin_address;
    }

    auto stack_size = ctx.High - ctx.Low;

    // we should allocate memory for new stack copy if it was't allocated yet
    // or current stack size isn't big enough or current stack size is too big
    if (stack_size > std::get<1>(ctx.Stack) || (stack_size << 1) < std::get<1>(ctx.Stack)) {
        delete[] std::get<0>(ctx.Stack);
        std::get<0>(ctx.Stack) = new char[stack_size];
        std::get<1>(ctx.Stack) = stack_size;
    }
    
    memcpy(std::get<0>(ctx.Stack), ctx.Low, stack_size);
}

void Engine::Restore(context &ctx) {
    char begin_address;
    // saving stack of restored coroutine
    while (&begin_address <= ctx.High && &begin_address >= ctx.Low) {
        Restore(ctx);
    }
    // now we can restore coroutine's stack without changing our stack
    std::memcpy(ctx.Low, std::get<0>(ctx.Stack), ctx.High - ctx.Low);
    cur_coro = &ctx;
    // run coroutine from the point where it was stopped
    longjmp(ctx.Environment, 1);
}

void Engine::enter(Engine::context *ctx) {
    assert(cur_coro != nullptr);
    if (cur_coro != idle_ctx) {
        if (setjmp(cur_coro->Environment) > 0) {
            return;
        }
        Store(*cur_coro);
    }
    Restore(*ctx);
}

void Engine::yield() {
    // there are no alive coroutines 
    // or there is only one alive coroutine, which
    // is the current one
    if (!alive || (cur_coro == alive && !alive->next)) {
    	return;
    }
    
    context *next_coro = alive;
    if (cur_coro == alive) {
        next_coro = alive->next;
    }
    // running the next alive coroutine
    enter(next_coro);
}

void Engine::sched(void *coro) {
    auto next_coro = static_cast<context *>(coro);
    if (next_coro == nullptr) {
        yield();
    }
    
    if (next_coro->is_blocked || next_coro == cur_coro) {
        return;
    }
    
    enter(next_coro);
}

void Engine::delete_elem(context *&head, context *&elem) {
    if (head == elem) {
        head = head->next;
    }
    if (elem->prev != nullptr) {
        elem->prev->next = elem->next;
    }
    if (elem->next != nullptr) {
        elem->next->prev = elem->prev;
    }
}

void Engine::add_elem_to_head(context *&head, context *&new_head) {
    if (head == nullptr) {
        head = new_head;
        head->next = nullptr;
    } else {
        head->prev = new_head;
        new_head->next = head;
        head = new_head;
    }

    head->prev = nullptr;
}

void Engine::block(void *coro) {
    context *coro_to_block = cur_coro;
    
    if (coro) {
        coro_to_block = static_cast<context *>(coro);
    }

    if (!coro_to_block || coro_to_block->is_blocked) {
        return;
    }
    coro_to_block->is_blocked = true;

    // delete coroutine from the list of alive coroutines
    delete_elem(alive, coro_to_block);
    // add coroutine to the list of blocked coroutines
    add_elem_to_head(blocked, coro_to_block);

    if (coro_to_block == cur_coro) {
        enter(idle_ctx);
    }
}

void Engine::unblock(void *coro) {
    auto coro_to_unblock = static_cast<context *>(coro);
    // we shouldn't unblock coroutine if it's already unblocked
    if (!coro_to_unblock || !coro_to_unblock->is_blocked) {
        return;
    }
    coro_to_unblock->is_blocked = false;

    // delete coroutine from the list of blocked coroutines
    delete_elem(blocked, coro_to_unblock);
    // add coroutine to the list of alive coroutines
    add_elem_to_head(alive, coro_to_unblock);
}

} // namespace Coroutine
} // namespace Afina
