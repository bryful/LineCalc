#include "Calculator.h"
#include <jni.h>
#include <codecvt>
#include <locale>
#include <memory>
#include <stdexcept>
#include <utility>
#include <vector>

namespace
{
    std::string bytes(JNIEnv* env, jbyteArray value)
    {
        const auto size = env->GetArrayLength(value);
        std::string result(static_cast<size_t>(size), '\0');
        env->GetByteArrayRegion(value, 0, size, reinterpret_cast<jbyte*>(result.data()));
        return result;
    }

    jstring javaText(JNIEnv* env, const std::wstring& value)
    {
        const auto utf8 = Calculator::toUtf8(value);
        const auto utf16 = std::wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t>{}.from_bytes(utf8);
        return env->NewString(reinterpret_cast<const jchar*>(utf16.data()), static_cast<jsize>(utf16.size()));
    }

    struct Session
    {
        std::string acorn;
        std::string script;
        std::filesystem::path definitions;
        bool clearHistory = false;
        std::vector<std::pair<std::wstring, std::wstring>> messages;
        std::unique_ptr<Calculator> calculator;

        CalculatorHost host(bool callbacks)
        {
            CalculatorHost result;
            result.loadResource = [this](int id) { return id == 201 ? acorn : script; };
            if (callbacks)
            {
                result.clearHistory = [this] { clearHistory = true; };
                result.showText = [this](const auto& title, const auto& text)
                {
                    if (messages.size() >= 16) throw std::runtime_error("一覧の表示回数が多すぎます。");
                    messages.emplace_back(title, text);
                };
            }
            return result;
        }
    };

    jobjectArray pack(JNIEnv* env, const Evaluation& result, const Session& session)
    {
        auto type = env->FindClass("java/lang/String");
        auto array = env->NewObjectArray(static_cast<jsize>(5 + session.messages.size() * 2), type, nullptr);
        env->DeleteLocalRef(type);
        const auto put = [&](int index, const std::wstring& value)
        {
            auto text = javaText(env, value);
            env->SetObjectArrayElement(array, index, text);
            env->DeleteLocalRef(text);
        };
        put(0, result.success ? L"1" : L"0");
        put(1, result.text);
        put(2, result.error);
        put(3, result.reset ? L"1" : L"0");
        put(4, session.clearHistory ? L"1" : L"0");
        int index = 5;
        for (const auto& [title, text] : session.messages)
        {
            put(index++, title);
            put(index++, text);
        }
        return array;
    }

    void fail(JNIEnv* env, const std::exception& error)
    {
        const auto type = env->FindClass("java/lang/IllegalStateException");
        env->ThrowNew(type, error.what());
        env->DeleteLocalRef(type);
    }
}

extern "C" JNIEXPORT jlong JNICALL
Java_io_github_bryful_linecalc_NativeCalculator_create(JNIEnv* env, jclass, jbyteArray acorn, jbyteArray runtime, jbyteArray path)
{
    try
    {
        auto session = std::make_unique<Session>();
        session->acorn = bytes(env, acorn);
        session->script = bytes(env, runtime);
        session->definitions = std::filesystem::path(bytes(env, path));
        session->calculator = std::make_unique<Calculator>(session->host(true));
        session->calculator->initialize();
        return reinterpret_cast<jlong>(session.release());
    }
    catch (const std::exception& error) { fail(env, error); return 0; }
}

extern "C" JNIEXPORT jobjectArray JNICALL
Java_io_github_bryful_linecalc_NativeCalculator_execute(JNIEnv* env, jclass, jlong handle, jbyteArray input, jint action, jint memory)
{
    try
    {
        auto& session = *reinterpret_cast<Session*>(handle);
        session.clearHistory = false;
        session.messages.clear();
        Evaluation result;
        if (action == 1)
        {
            session.calculator->initialize();
            result = session.calculator->loadDefinitions(session.definitions);
            result.reset = true;
        }
        else if (action == 2)
        {
            Calculator validator(session.host(false));
            validator.initialize();
            result = validator.loadDefinitions(session.definitions);
        }
        else
        {
            result = session.calculator->evaluate(Calculator::fromUtf8(bytes(env, input)));
            if (action == 3 && result.success)
            {
                if (result.number && memory >= 0 && memory < 12)
                    session.calculator->storeMemory(memory, *result.number);
                else
                {
                    result.success = false;
                    result.error = L"メモリ保存には数値の計算結果が必要です。";
                }
            }
        }
        return pack(env, result, session);
    }
    catch (const std::exception& error) { fail(env, error); return nullptr; }
}

extern "C" JNIEXPORT jobjectArray JNICALL
Java_io_github_bryful_linecalc_NativeCalculator_memories(JNIEnv* env, jclass, jlong handle)
{
    try
    {
        auto& session = *reinterpret_cast<Session*>(handle);
        auto type = env->FindClass("java/lang/String");
        auto array = env->NewObjectArray(12, type, nullptr);
        env->DeleteLocalRef(type);
        for (int i = 0; i < 12; ++i)
        {
            auto text = javaText(env, session.calculator->memoryText(i));
            env->SetObjectArrayElement(array, i, text);
            env->DeleteLocalRef(text);
        }
        return array;
    }
    catch (const std::exception& error) { fail(env, error); return nullptr; }
}

extern "C" JNIEXPORT void JNICALL
Java_io_github_bryful_linecalc_NativeCalculator_destroy(JNIEnv*, jclass, jlong handle)
{
    delete reinterpret_cast<Session*>(handle);
}
