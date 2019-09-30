/*
 * File: albert_example_parser.cc
 * Project: bert
 * Author: koth (Koth Chen)
 * -----
 * Last Modified: 2019-09-27 4:56:56
 * Modified By: koth (nobody@verycool.com)
 * -----
 * Copyright 2020 - 2019
 */

#include "bert/albert_example_parser.h"

#include "absl/strings/ascii.h"
#include "sentencepiece/sentencepiece_processor.h"
#include "utils/logging.h"

namespace radish {
// 200 -1
static int kMaxLen = 199;
static int kMaxLabel = 28;

ALBertExampleParser::ALBertExampleParser()
    : gen_(std::random_device{}()),
      len_dist_({6, 3, 2}),
      random_p_dist_(0, 1) {}

bool ALBertExampleParser::_mask_seq(int maskId, int sepId, int totalVocabSize,
                                    int len, Ex& ex) {
  std::vector<int> offs;
  for (int i = 0; i < len; i += 4) {
    offs.push_back(i);
  }
  std::random_shuffle(offs.begin(), offs.end());
  std::vector<bool> masked(len, false);
  int num_masked = 0;
  for (auto p : offs) {
    int off = p * 4;
    if (num_masked >= kMaxLabel) {
      break;
    }
    int drawLen = len_dist_(gen_) + 1;
    if (drawLen + num_masked > kMaxLabel) {
      continue;
    }
    std::uniform_int_distribution<> off_dist(0, 5 - drawLen);
    off += off_dist(gen_);

    if (off == 0) {
      // 第一个位置留给 CLS, 不mask
      continue;
    }
    bool valid = true;
    for (int i = 0; i < drawLen; i++) {
      if ((i + off) >= len || masked[i + off] || ex.x[i + off] == sepId) {
        valid = false;
        break;
      }
    }
    // actually mask
    if (valid) {
      for (int i = 0; i < drawLen; i++) {
        masked[i + off] = true;
      }
      num_masked += drawLen;
    }
  }
  for (int i = 0; i < len; i++) {
    if (masked[i]) {
      int toReplace = 0;
      float p = random_p_dist_(gen_);
      float p2 = random_p_dist_(gen_);
      if (p > 0.8) {
        if (p2 <= 0.5) {
          toReplace = random_id_dist_(gen_);
        }
      } else {
        toReplace = maskId;
      }
      ex.target.push_back(ex.x[i]);
      ex.indexies.push_back(i);
      if (toReplace != 0) {
        ex.x[i] = toReplace;
      }
    }
  }
  CHECK_LE(static_cast<int>(ex.target.size()), kMaxLabel);
  for (int i = ex.target.size(); i < kMaxLabel; i++) {
    ex.target.push_back(0);
    ex.indexies.push_back(0);
  }
  return true;
}
ALBertExampleParser::~ALBertExampleParser() = default;
bool ALBertExampleParser::Init(const Json::Value& config) {
  std::string spm_model_path = config.get("spm_model_path", "").asString();
  spdlog::info("got spm_model_path:{}", spm_model_path);
  if (spm_model_path.empty()) {
    return false;
  }
  spp_.reset(new sentencepiece::SentencePieceProcessor());
  if (!spp_->Load(spm_model_path).ok()) {
    return false;
  }
  int totalVocab = spp_->GetPieceSize();
  random_id_dist_ = std::uniform_int_distribution<>(1, totalVocab - 1);
  return true;
}

bool ALBertExampleParser::ParseOne(std::string line,
                                   data::LlbExample& example) {
  absl::RemoveExtraAsciiWhitespace(&line);
  std::string x = absl::AsciiStrToLower(line);
  auto ids = spp_->EncodeAsIds(x);
  int totalVocabSize = spp_->GetPieceSize();
  Ex ex(kMaxLen + 1);
  int clsId = totalVocabSize;
  int maskId = totalVocabSize + 1;
  int sepId = totalVocabSize + 2;
  ex.x[0] = clsId;
  if (random_p_dist_(gen_) <= 0.5) {
    ex.ordered = 1;
  } else {
    ex.ordered = 0;
  }
  int total = ids.size();
  std::discrete_distribution<> ranoff_p(0, 3);
  int off = ranoff_p(gen_);
  int end = total;
  if ((end - off) > kMaxLen - 1) {
    end = off + kMaxLen - 1;
  }
  int thirdthLen = (end - off) / 3;
  if (thirdthLen < 5) {
    spdlog::warn("too short to be an example:{}, text=[{}]", end - off, x);
    return false;
  }
  std::discrete_distribution<> ranmid_p(thirdthLen, 2 * thirdthLen - 1);
  int mid = ranmid_p(gen_);
  int k = 1;
  if (ex.ordered) {
    for (int i = off; i < mid; i++) {
      ex.x[k] = ids[i];
      ex.types[k] = 1;
      k += 1;
    }
    ex.types[k] = 1;
    ex.x[k] = sepId;
    k += 1;
    // mid has place SEP
    for (int i = mid + 1; i < end; i++) {
      ex.x[k] = ids[i];
      ex.types[k] = 2;
      k += 1;
    }
  } else {
    for (int i = mid + 1; i < end; i++) {
      ex.x[k] = ids[i];
      ex.types[k] = 1;
      k += 1;
    }
    ex.types[k] = 1;
    ex.x[k] = sepId;
    k += 1;
    for (int i = off; i < mid; i++) {
      ex.x[k] = ids[i];
      ex.types[k] = 2;
      k += 1;
    }
  }
  if (!_mask_seq(maskId, sepId, totalVocabSize, k, ex)) {
    spdlog::warn("mask example error");
    return false;
  }
  example.features.push_back(
      torch::tensor(ex.x, at::dtype(torch::kInt64).requires_grad(false)));
  example.features.push_back(torch::tensor(
      ex.indexies, at::dtype(torch::kInt64).requires_grad(false)));
  example.features.push_back(
      torch::tensor(ex.types, at::dtype(torch::kInt64).requires_grad(false)));
  example.features.push_back(
      torch::tensor(ex.ordered, at::dtype(torch::kInt64).requires_grad(false)));
  example.target =
      torch::tensor(ex.target, at::dtype(torch::kInt64).requires_grad(false));
  return true;
}
}  // namespace radish