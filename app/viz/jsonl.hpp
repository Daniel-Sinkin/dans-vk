// app/viz/jsonl.hpp
//
// JSONL / JSON envelope file loader. Each .jsonl line is one envelope;
// .json files are one envelope. Ported from loadFolder in the HTML.
//
#pragma once

#include "app/viz/envelope.hpp"
// StdLib
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
//

namespace viz
{

struct LoadedFrames
{
    std::vector<Frame> frames{};
    std::string source{};
};

[[nodiscard]] inline auto read_text(const std::filesystem::path& path) -> std::string
{
    std::ifstream file(path);
    if (not file)
    {
        throw std::runtime_error("cannot open file: " + path.string());
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

[[nodiscard]] inline auto load_envelope_file(
    const std::filesystem::path& path, Vec2 stage_center
) -> std::vector<Frame>
{
    const auto text = read_text(path);
    const auto name = path.filename().string();
    std::vector<Frame> out;
    if (path.extension() == ".jsonl")
    {
        std::stringstream ss(text);
        std::string line;
        i32 li = 0;
        while (std::getline(ss, line))
        {
            usize start = 0;
            while (start < line.size() and std::isspace(static_cast<unsigned char>(line[start])))
            {
                ++start;
            }
            usize end = line.size();
            while (end > start and std::isspace(static_cast<unsigned char>(line[end - 1])))
            {
                --end;
            }
            if (end <= start)
            {
                continue;
            }
            ++li;
            try
            {
                const auto env = json::parse(line.substr(start, end - start));
                out.push_back(build_frame(env, name + ":" + std::to_string(li), stage_center));
            }
            catch (const std::exception& e)
            {
                throw std::runtime_error(
                    name + " line " + std::to_string(li) + ": " + e.what()
                );
            }
        }
    }
    else
    {
        try
        {
            const auto env = json::parse(text);
            out.push_back(build_frame(env, name, stage_center));
        }
        catch (const std::exception& e)
        {
            throw std::runtime_error(name + ": " + e.what());
        }
    }
    return out;
}

[[nodiscard]] inline auto load_envelope_folder(
    const std::filesystem::path& folder, Vec2 stage_center
) -> LoadedFrames
{
    LoadedFrames result;
    result.source = folder.filename().string();
    std::vector<std::filesystem::path> files;
    if (std::filesystem::is_directory(folder))
    {
        for (const auto& entry : std::filesystem::directory_iterator(folder))
        {
            if (not entry.is_regular_file())
            {
                continue;
            }
            const auto ext = entry.path().extension();
            if (ext == ".json" or ext == ".jsonl")
            {
                files.push_back(entry.path());
            }
        }
        std::sort(files.begin(), files.end());
    }
    else
    {
        files.push_back(folder);
    }
    for (const auto& f : files)
    {
        auto frames = load_envelope_file(f, stage_center);
        for (auto& frame : frames)
        {
            result.frames.push_back(std::move(frame));
        }
    }
    return result;
}

}  // namespace viz
