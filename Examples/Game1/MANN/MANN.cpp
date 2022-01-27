#include "MANN.hpp"

#include <iostream>

MANN::MANN(const std::filesystem::path& model_path)
    : m_Env(OrtLoggingLevel::ORT_LOGGING_LEVEL_WARNING, "mann"),
      m_Session(m_Env, model_path.c_str(), Ort::SessionOptions{}) {
    Ort::AllocatorWithDefaultOptions allocator;
    m_InputName  = std::string(m_Session.GetInputName(0, allocator));
    m_OutputName = std::string(m_Session.GetOutputName(0, allocator));
}

std::vector<float> MANN::Predict(std::vector<float>& input) {
    std::vector<float> result(input.size());

    auto input_name  = m_InputName.c_str();
    auto output_name = m_OutputName.c_str();

    std::array<int64_t, 2> shape = {1, static_cast<int64_t>(input.size())};

    auto memory_info   = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
    auto input_tensor  = Ort::Value::CreateTensor<float>(memory_info, input.data(), input.size(), shape.data(), shape.size());
    auto output_tensor = Ort::Value::CreateTensor<float>(memory_info, result.data(), result.size(), shape.data(), shape.size());
    auto p             = input_tensor.GetTensorMutableData<float>();
    m_Session.Run(Ort::RunOptions{nullptr}, &input_name, &input_tensor, 1, &output_name, &output_tensor, 1);

    return result;
}
