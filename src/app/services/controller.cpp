#include "controller.hpp"

namespace otto::services {

  using namespace drivers;

  struct ExecutorWrapper : InputHandler, LogicDomain {
    ExecutorWrapper(util::smart_ptr<IInputHandler>&& delegate) : delegate_(std::move(delegate)) {}

    void handle(KeyPress e) noexcept override
    {
      executor().execute([h = delegate_.get(), e] { h->handle(e); });
    }
    void handle(KeyRelease e) noexcept override
    {
      executor().execute([h = delegate_.get(), e] { h->handle(e); });
    }
    void handle(EncoderEvent e) noexcept override
    {
      executor().execute([h = delegate_.get(), e] { h->handle(e); });
    }

  private:
    util::smart_ptr<IInputHandler> delegate_;
  };

  Controller::Controller(RuntimeController& rt, Config conf, util::smart_ptr<drivers::MCUPort>&& port)
    : conf_(conf), com_(rt, std::move(port)), thread_([this, &rt](const std::stop_token& stop_token) {
        std::vector<Packet> second_buf;
        while (!stop_token.stop_requested()) {
          second_buf.clear();
          {
            std::unique_lock l(queue_mutex_);
            std::swap(queue_, second_buf);
          }
          for (const auto& data : second_buf) {
            com_.port_->write(data);
          }
          Packet p;
          do {
            p = com_.port_->read();
            com_.handle_packet(p);
          } while (p.cmd != Command::none);
          std::this_thread::sleep_for(conf_.wait_time);
        }
      })
  {}

  void Controller::set_input_handler(IInputHandler& h)
  {
    com_.handler = std::make_unique<ExecutorWrapper>(&h);
  }

  MCUCommunicator::MCUCommunicator(RuntimeController& rt, util::smart_ptr<MCUPort>&& port)
    : rt_(rt), port_(std::move(port))
  {}

  void MCUCommunicator::handle_packet(Packet p)
  {
    if (!handler) return;
    switch (p.cmd) {
      case Command::key_events: {
        std::span<std::uint8_t> presses = {p.data.data(), 8};
        std::span<std::uint8_t> releases = {p.data.data() + 8, 8};
        for (auto k : util::enum_values<Key>()) {
          if (util::get_bit(presses, util::enum_integer(k))) handler->handle(KeyPress{{k}});
          if (util::get_bit(releases, util::enum_integer(k))) handler->handle(KeyRelease{{k}});
        }
      } break;
      case Command::encoder_events: {
        for (auto e : util::enum_values<Encoder>()) {
          if (p.data[util::enum_integer(e)] == 0) continue;
          handler->handle(EncoderEvent{e, static_cast<std::int8_t>(p.data[util::enum_integer(e)])});
        }
      } break;
      case Command::shutdown: {
        rt_.request_stop();
      } break;
      default: break;
    }
  }
} // namespace otto::services
