// app/viz/julia_client.hpp
//
// HTTP client for the Julia server.jl: POST /contract and GET /healthz.
//
#pragma once

#include "app/viz/envelope.hpp"
// Externals
#define CPPHTTPLIB_NO_EXCEPTIONS
#include <httplib.h>
#include <nlohmann/json.hpp>
// StdLib
#include <chrono>
#include <optional>
#include <string>
#include <utility>
//

namespace viz
{

struct JuliaClientConfig
{
    std::string host{"127.0.0.1"};
    int port{8754};
    int timeout_seconds{10};
};

struct ContractResult
{
    bool ok{};
    Frame frame{};
    std::string error{};
};

class JuliaClient
{
  public:
    explicit JuliaClient(JuliaClientConfig config)
        : config_(std::move(config)), client_(config_.host, config_.port)
    {
        client_.set_connection_timeout(config_.timeout_seconds, 0);
        client_.set_read_timeout(config_.timeout_seconds, 0);
        client_.set_write_timeout(config_.timeout_seconds, 0);
    }

    [[nodiscard]] auto healthz() -> bool
    {
        const auto resp = client_.Get("/healthz");
        return resp and resp->status == 200;
    }

    [[nodiscard]] auto contract(
        const Abstraction& left_abs,
        const Abstraction& right_abs,
        const std::vector<Tensor>& tensors,
        std::string order,
        Vec2 stage_center
    ) -> ContractResult
    {
        ContractResult result;
        json payload;
        try
        {
            payload["left"] = container_to_chain_envelope(left_abs, tensors);
            payload["right"] = container_to_chain_envelope(right_abs, tensors);
            payload["order"] = std::move(order);
        }
        catch (const std::exception& e)
        {
            result.error = std::string{"envelope build failed: "} + e.what();
            return result;
        }

        const auto body = payload.dump();
        const auto resp = client_.Post("/contract", body, "application/json");
        if (not resp)
        {
            result.error = "Julia server unreachable";
            return result;
        }
        if (resp->status != 200)
        {
            result.error = "HTTP " + std::to_string(resp->status) + ": " + resp->body;
            return result;
        }
        try
        {
            const auto reply = json::parse(resp->body);
            result.frame
                = build_frame(reply, left_abs.name + " x " + right_abs.name, stage_center);
            result.ok = true;
        }
        catch (const std::exception& e)
        {
            result.error = std::string{"failed to parse response: "} + e.what();
        }
        return result;
    }

  private:
    JuliaClientConfig config_{};
    httplib::Client client_;
};

}  // namespace viz
