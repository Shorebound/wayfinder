#include "core/events/ApplicationEvent.h"
#include "core/events/EventQueue.h"
#include "core/events/KeyEvent.h"
#include "core/events/MouseEvent.h"
#include "platform/KeyCodes.h"

#include <doctest/doctest.h>

#include <vector>

// ── EventQueue Basics ────────────────────────────────────
namespace Wayfinder::Tests
{
    TEST_CASE("Default-constructed queue is empty")
    {
        Wayfinder::EventQueue queue;
        CHECK(queue.IsEmpty());
        CHECK(queue.Size() == 0);
    }

    TEST_CASE("Push increases size")
    {
        Wayfinder::EventQueue queue;
        queue.Push(Wayfinder::WindowCloseEvent{});
        CHECK_FALSE(queue.IsEmpty());
        CHECK(queue.Size() == 1);

        queue.Push(Wayfinder::WindowCloseEvent{});
        CHECK(queue.Size() == 2);
    }

    // ── Drain ────────────────────────────────────────────────

    TEST_CASE("Drain delivers events in FIFO order")
    {
        Wayfinder::EventQueue queue;
        queue.Push(Wayfinder::KeyPressedEvent{Wayfinder::Key::A});
        queue.Push(Wayfinder::KeyReleasedEvent{Wayfinder::Key::B});
        queue.Push(Wayfinder::MouseMovedEvent{10.0f, 20.0f});

        std::vector<Wayfinder::EventType> received;
        queue.Drain([&](Wayfinder::Event& e)
        {
            received.push_back(e.GetEventType());
        });

        REQUIRE(received.size() == 3);
        CHECK(received[0] == Wayfinder::EventType::KeyPressed);
        CHECK(received[1] == Wayfinder::EventType::KeyReleased);
        CHECK(received[2] == Wayfinder::EventType::MouseMoved);
    }

    TEST_CASE("Queue is empty after Drain")
    {
        Wayfinder::EventQueue queue;
        queue.Push(Wayfinder::WindowCloseEvent{});
        queue.Push(Wayfinder::WindowCloseEvent{});

        queue.Drain([](Wayfinder::Event&)
        {
        });

        CHECK(queue.IsEmpty());
        CHECK(queue.Size() == 0);
    }

    TEST_CASE("Events queued during Drain are deferred to the next drain cycle")
    {
        Wayfinder::EventQueue queue;
        queue.Push(Wayfinder::KeyPressedEvent{Wayfinder::Key::A});

        std::vector<Wayfinder::KeyCode> received;
        queue.Drain([&](Wayfinder::Event& e)
        {
            auto& typed = static_cast<Wayfinder::KeyPressedEvent&>(e);
            received.push_back(typed.GetKeyCode());

            if (typed.GetKeyCode() == Wayfinder::Key::A)
            {
                queue.Push(Wayfinder::KeyPressedEvent{Wayfinder::Key::B});
            }
        });

        REQUIRE(received.size() == 1);
        CHECK(received[0] == Wayfinder::Key::A);
        CHECK(queue.Size() == 1);

        queue.Drain([&](Wayfinder::Event& e)
        {
            auto& typed = static_cast<Wayfinder::KeyPressedEvent&>(e);
            received.push_back(typed.GetKeyCode());
        });

        REQUIRE(received.size() == 2);
        CHECK(received[1] == Wayfinder::Key::B);
        CHECK(queue.IsEmpty());
    }

    TEST_CASE("Drain on empty queue does not invoke handler")
    {
        Wayfinder::EventQueue queue;
        int callCount = 0;
        queue.Drain([&](Wayfinder::Event&)
        {
            ++callCount;
        });
        CHECK(callCount == 0);
    }

    // ── Clear ────────────────────────────────────────────────

    TEST_CASE("Clear discards events without dispatching")
    {
        Wayfinder::EventQueue queue;
        queue.Push(Wayfinder::KeyPressedEvent{Wayfinder::Key::A});
        queue.Push(Wayfinder::KeyPressedEvent{Wayfinder::Key::B});

        queue.Clear();

        CHECK(queue.IsEmpty());

        int callCount = 0;
        queue.Drain([&](Wayfinder::Event&)
        {
            ++callCount;
        });
        CHECK(callCount == 0);
    }

    // ── Handled flag propagation through Drain ───────────────

    TEST_CASE("Handler receives mutable event reference during Drain")
    {
        Wayfinder::EventQueue queue;
        queue.Push(Wayfinder::KeyPressedEvent{Wayfinder::Key::A});

        bool wasHandled = false;
        queue.Drain([&](Wayfinder::Event& e)
        {
            e.Handled = true;
            wasHandled = e.Handled;
        });

        CHECK(wasHandled);
    }

    // ── Multiple drain cycles ────────────────────────────────

    TEST_CASE("Queue can be reused across multiple drain cycles")
    {
        Wayfinder::EventQueue queue;

        queue.Push(Wayfinder::KeyPressedEvent{Wayfinder::Key::A});
        int count1 = 0;
        queue.Drain([&](Wayfinder::Event&)
        {
            ++count1;
        });
        CHECK(count1 == 1);
        CHECK(queue.IsEmpty());

        queue.Push(Wayfinder::KeyPressedEvent{Wayfinder::Key::B});
        queue.Push(Wayfinder::KeyPressedEvent{Wayfinder::Key::C});
        int count2 = 0;
        queue.Drain([&](Wayfinder::Event&)
        {
            ++count2;
        });
        CHECK(count2 == 2);
        CHECK(queue.IsEmpty());
    }

    // ── Typed Read ───────────────────────────────────────────

    TEST_CASE("Read returns typed span of pending events")
    {
        Wayfinder::EventQueue queue;
        queue.Push(Wayfinder::KeyPressedEvent{Wayfinder::Key::A});
        queue.Push(Wayfinder::KeyPressedEvent{Wayfinder::Key::B, true});
        queue.Push(Wayfinder::MouseMovedEvent{1.0f, 2.0f});

        auto keys = queue.Read<Wayfinder::KeyPressedEvent>();
        REQUIRE(keys.size() == 2);
        CHECK(keys[0].GetKeyCode() == Wayfinder::Key::A);
        CHECK(keys[1].GetKeyCode() == Wayfinder::Key::B);
        CHECK(keys[1].IsRepeat() == true);

        auto mice = queue.Read<Wayfinder::MouseMovedEvent>();
        REQUIRE(mice.size() == 1);
        CHECK(mice[0].GetX() == doctest::Approx(1.0f));
        CHECK(mice[0].GetY() == doctest::Approx(2.0f));
    }

    TEST_CASE("Read returns empty span for absent event type")
    {
        Wayfinder::EventQueue queue;
        queue.Push(Wayfinder::KeyPressedEvent{Wayfinder::Key::A});

        auto mice = queue.Read<Wayfinder::MouseMovedEvent>();
        CHECK(mice.empty());
    }

    TEST_CASE("Read returns empty span after Drain")
    {
        Wayfinder::EventQueue queue;
        queue.Push(Wayfinder::KeyPressedEvent{Wayfinder::Key::A});
        queue.Drain([](Wayfinder::Event&)
        {
        });

        auto keys = queue.Read<Wayfinder::KeyPressedEvent>();
        CHECK(keys.empty());
    }

    // ── Contiguous value storage ─────────────────────────────

    TEST_CASE("Events are stored as values, not heap-allocated pointers")
    {
        Wayfinder::EventQueue queue;
        queue.Push(Wayfinder::KeyPressedEvent{Wayfinder::Key::Space, true});

        auto events = queue.Read<Wayfinder::KeyPressedEvent>();
        REQUIRE(events.size() == 1);
        CHECK(events[0].GetKeyCode() == Wayfinder::Key::Space);
        CHECK(events[0].IsRepeat() == true);
        CHECK(events[0].GetEventType() == Wayfinder::EventType::KeyPressed);
    }

    // ── Cross-type FIFO ordering ─────────────────────────────

    TEST_CASE("Interleaved event types maintain global FIFO order")
    {
        Wayfinder::EventQueue queue;
        queue.Push(Wayfinder::KeyPressedEvent{Wayfinder::Key::A});
        queue.Push(Wayfinder::MouseMovedEvent{5.0f, 10.0f});
        queue.Push(Wayfinder::KeyReleasedEvent{Wayfinder::Key::A});
        queue.Push(Wayfinder::MouseMovedEvent{15.0f, 20.0f});
        queue.Push(Wayfinder::KeyPressedEvent{Wayfinder::Key::B});

        std::vector<Wayfinder::EventType> order;
        queue.Drain([&](Wayfinder::Event& e)
        {
            order.push_back(e.GetEventType());
        });

        REQUIRE(order.size() == 5);
        CHECK(order[0] == Wayfinder::EventType::KeyPressed);
        CHECK(order[1] == Wayfinder::EventType::MouseMoved);
        CHECK(order[2] == Wayfinder::EventType::KeyReleased);
        CHECK(order[3] == Wayfinder::EventType::MouseMoved);
        CHECK(order[4] == Wayfinder::EventType::KeyPressed);
    }

    // ── Frame-boundary semantics ─────────────────────────────

    TEST_CASE("Drain acts as frame boundary — each cycle is independent")
    {
        Wayfinder::EventQueue queue;

        // Frame 1: push and drain
        queue.Push(Wayfinder::KeyPressedEvent{Wayfinder::Key::A});
        std::vector<Wayfinder::EventType> frame1;
        queue.Drain([&](Wayfinder::Event& e)
        {
            frame1.push_back(e.GetEventType());
        });
        CHECK(frame1.size() == 1);
        CHECK(queue.IsEmpty());

        // Frame 2: fresh set of events
        queue.Push(Wayfinder::MouseMovedEvent{1.0f, 2.0f});
        queue.Push(Wayfinder::KeyReleasedEvent{Wayfinder::Key::B});
        std::vector<Wayfinder::EventType> frame2;
        queue.Drain([&](Wayfinder::Event& e)
        {
            frame2.push_back(e.GetEventType());
        });
        REQUIRE(frame2.size() == 2);
        CHECK(frame2[0] == Wayfinder::EventType::MouseMoved);
        CHECK(frame2[1] == Wayfinder::EventType::KeyReleased);
        CHECK(queue.IsEmpty());

        // Frame 3: empty frame
        std::vector<Wayfinder::EventType> frame3;
        queue.Drain([&](Wayfinder::Event& e)
        {
            frame3.push_back(e.GetEventType());
        });
        CHECK(frame3.empty());
    }

    // ── Buffer rollover ──────────────────────────────────────

    TEST_CASE("Buffers handle many push-drain cycles without leaking state")
    {
        Wayfinder::EventQueue queue;

        for (int cycle = 0; cycle < 100; ++cycle)
        {
            queue.Push(Wayfinder::KeyPressedEvent{Wayfinder::Key::A});
            queue.Push(Wayfinder::MouseMovedEvent{static_cast<float>(cycle), static_cast<float>(cycle)});

            int count = 0;
            queue.Drain([&](Wayfinder::Event&)
            {
                ++count;
            });
            CHECK(count == 2);
            CHECK(queue.IsEmpty());
        }
    }
}
