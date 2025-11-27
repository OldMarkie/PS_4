#pragma once
#include <tesseract/baseapi.h>
#include <leptonica/allheaders.h>
#include <string>

class OCRWorker {
public:
    OCRWorker();
    ~OCRWorker();
    std::string process(const std::string& filename, const std::byte* data, size_t size);

private:
    tesseract::TessBaseAPI api_;
};

OCRWorker::OCRWorker() {
    if (api_.Init(nullptr, "eng_fast")) {
        throw std::runtime_error("Tesseract init failed");
    }
}

OCRWorker::~OCRWorker() { api_.End(); }

std::string OCRWorker::process(const std::string& filename, const std::byte* data, size_t size) {
    Pix* pix = pixReadMem(reinterpret_cast<const unsigned char*>(data), size);
    if (!pix) return "<PIX_READ_FAILED>";

    // Same preprocessing you had in PS3
    Pix* gray = pixConvertTo8(pix, false);
    Pix* bin = nullptr;
    pixOtsuAdaptiveThreshold(gray, 200, 200, 25, 25, 0.1f, nullptr, &bin);
    pixDestroy(&pix); pixDestroy(&gray);

    api_.SetImage(bin);
    char* out = api_.GetUTF8Text();
    std::string result = out ? out : "";
    delete[] out;

    pixDestroy(&bin);
    return result;
}