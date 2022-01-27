#pragma once
#include <vector>
#include <filesystem>

#include <onnxruntime_cxx_api.h>

class MANN {
public:
    MANN(const std::filesystem::path& model_path);

    std::vector<float> Predict(std::vector<float>& input);

private:
    Ort::Env     m_Env;
    Ort::Session m_Session;

    std::string m_InputName;
    std::string m_OutputName;
};