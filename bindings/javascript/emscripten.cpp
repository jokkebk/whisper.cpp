#include "whisper.h"

#include <emscripten.h>
#include <emscripten/bind.h>

#include <vector>
#include <thread>

std::vector<struct whisper_context *> g_contexts(4, nullptr);

EMSCRIPTEN_BINDINGS(whisper) {
    emscripten::function("init", emscripten::optional_override([](const std::string & path_model) {
        for (size_t i = 0; i < g_contexts.size(); ++i) {
            if (g_contexts[i] == nullptr) {
                g_contexts[i] = whisper_init(path_model.c_str());
                if (g_contexts[i] != nullptr) {
                    return i + 1;
                } else {
                    return (size_t) 0;
                }
            }
        }

        return (size_t) 0;
    }));

    emscripten::function("free", emscripten::optional_override([](size_t index) {
        --index;

        if (index < g_contexts.size()) {
            whisper_free(g_contexts[index]);
            g_contexts[index] = nullptr;
        }
    }));

    emscripten::function("full_default", emscripten::optional_override([](size_t index, const emscripten::val & audio, const std::string & lang, bool translate) {
        --index;

        if (index >= g_contexts.size()) {
            return -1;
        }

        if (g_contexts[index] == nullptr) {
            return -2;
        }

        struct whisper_full_params params = whisper_full_default_params(whisper_sampling_strategy::WHISPER_SAMPLING_GREEDY);

        printf("full_default: available threads %d\n", std::thread::hardware_concurrency());

        params.print_realtime       = true;
        params.print_progress       = false;
        params.print_timestamps     = true;
        params.print_special_tokens = false;
        params.translate            = translate;
        params.language             = whisper_is_multilingual(g_contexts[index]) ? lang.c_str() : "en";
        params.n_threads            = std::min(8, (int) std::thread::hardware_concurrency());
        params.offset_ms            = 0;

        printf("full_default: using %d threads\n", params.n_threads);
        printf("full_default: language '%s'\n", params.language);

        std::vector<float> pcmf32;
        const int n = audio["length"].as<int>();

        emscripten::val heap = emscripten::val::module_property("HEAPU8");
        emscripten::val memory = heap["buffer"];

        pcmf32.resize(n);

        emscripten::val memoryView = audio["constructor"].new_(memory, reinterpret_cast<uintptr_t>(pcmf32.data()), n);
        memoryView.call<void>("set", audio);

        int ret = whisper_full(g_contexts[index], params, pcmf32.data(), pcmf32.size());

        whisper_print_timings(g_contexts[index]);

        return ret;
    }));
}
