#include "core/events/EventQueue.h"
#include "core/events/ApplicationEvent.h"
#include "core/events/KeyEvent.h"
#include "core/events/MouseEvent.h"
#include "core/KeyCodes.h"

#include <doctest/doctest.h>

#include <vector>

// ── EventQueue Basics ────────────────────────────────────

TEST_CASE("Default-constructed queue is empty")
{
    Wayfinder::EventQueue queue;
    CHECK(queue.IsEmpty());
    CHECK(queue.Size() == 0);
}

TEST_CASE("Push increases size")
{
    Wayfinder::EventQueue queue;
    queue.Push(std::make_unique<Wayfinder::WindowCloseEvent>());
    CHECK_FALSE(queue.IsEmpty());
    CHECK(queue.Size() == 1);

    queue.Push(std::make_unique<Wayfinder::WindowCloseEvent>());
    CHECK(queue.Size() == 2);
}

// ── Drain ────────────────────────────────────────────────

TEST_CASE("Drain delivers events in FIFO order")
{
    Wayfinder::EventQueue queue;
    queue.Push(std::make_unique<Wayfinder::KeyPressedEvent>(Wayfinder::Key::A));
    queue.Push(std::make_unique<Wayfinder::KeyReleasedEvent>(Wayfinder::Key::B));
    queue.Push(std::make_unique<Wayfinder::MouseMovedEvent>(10.0f, 20.0f));

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
    queue.Push(std::make_unique<Wayfinder::WindowCloseEvent>());
    queue.Push(std::make_unique<Wayfinder::WindowCloseEvent>());

    queue.Drain([](Wayfinder::Event&) {});

    CHECK(queue.IsEmpty());
    CHECK(queue.Size() == 0);
}

TEST_CASE("Drain on empty queue does not invoke handler")
{
    Wayfinder::EventQueue queue;
    int callCount = 0;
    queue.Drain([&](Wayfinder::Event&) { ++callCount; });
    CHECK(callCount == 0);
}

// ── Clear ────────────────────────────────────────────────

TEST_CASE("Clear discards events without dispatching")
{
    Wayfinder::EventQueue queue;
    queue.Push(std::make_unique<Wayfinder::KeyPressedEvent>(Wayfinder::Key::A));
    queue.Push(std::make_unique<Wayfinder::KeyPressedEvent>(Wayfinder::Key::B));

    queue.Clear();

    CHECK(queue.IsEmpty());

    int callCount = 0;
    queue.Drain([&](Wayfinder::Event&) { ++callCount; });
    CHECK(callCount == 0);
}

// ── Event::Clone ─────────────────────────────────────────

TEST_CASE("Clone preserves event type and data for KeyPressedEvent")
{
    Wayfinder::KeyPressedEvent original(Wayfinder::Key::Space, true);
    auto cloned = original.Clone();

    REQUIRE(cloned != nullptr);
    CHECK(cloned->GetEventType() == Wayfinder::EventType::KeyPressed);
    CHECK(cloned->IsInCategory(Wayfinder::EventCategory::Input));

    auto& typed = static_cast<Wayfinder::KeyPressedEvent&>(*cloned);
    CHECK(typed.GetKeyCode() == Wayfinder::Key::Space);
    CHECK(typed.IsRepeat() == true);
}

TEST_CASE("Clone preserves event type and data for MouseMovedEvent")
{
    Wayfinder::MouseMovedEvent original(42.5f, 99.0f);
    auto cloned = original.Clone();

    REQUIRE(cloned != nullptr);
    CHECK(cloned->GetEventType() == Wayfinder::EventType::MouseMoved);

    auto& typed = static_cast<Wayfinder::MouseMovedEvent&>(*cloned);
    CHECK(typed.GetX() == doctest::Approx(42.5f));
    CHECK(typed.GetY() == doctest::Approx(99.0f));
}

TEST_CASE("Clone preserves event type and data for WindowResizeEvent")
{
    Wayfinder::WindowResizeEvent original(1920, 1080);
    auto cloned = original.Clone();

    REQUIRE(cloned != nullptr);
    CHECK(cloned->GetEventType() == Wayfinder::EventType::WindowResize);

    auto& typed = static_cast<Wayfinder::WindowResizeEvent&>(*cloned);
    CHECK(typed.GetWidth() == 1920);
    CHECK(typed.GetHeight() == 1080);
}

TEST_CASE("Cloned event is independent of original")
{
    Wayfinder::KeyPressedEvent original(Wayfinder::Key::A);
    auto cloned = original.Clone();

    original.Handled = true;
    CHECK_FALSE(cloned->Handled);
}

// ── Handled flag propagation through Drain ───────────────

TEST_CASE("Handler receives mutable event reference during Drain")
{
    Wayfinder::EventQueue queue;
    queue.Push(std::make_unique<Wayfinder::KeyPressedEvent>(Wayfinder::Key::A));

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

    queue.Push(std::make_unique<Wayfinder::KeyPressedEvent>(Wayfinder::Key::A));
    int count1 = 0;
    queue.Drain([&](Wayfinder::Event&) { ++count1; });
    CHECK(count1 == 1);
    CHECK(queue.IsEmpty());

    queue.Push(std::make_unique<Wayfinder::KeyPressedEvent>(Wayfinder::Key::B));
    queue.Push(std::make_unique<Wayfinder::KeyPressedEvent>(Wayfinder::Key::C));
    int count2 = 0;
    queue.Drain([&](Wayfinder::Event&) { ++count2; });
    CHECK(count2 == 2);
    CHECK(queue.IsEmpty());
}
