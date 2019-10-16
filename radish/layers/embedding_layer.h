#pragma once

#include <torch/nn/cloneable.h>
#include <torch/nn/pimpl.h>
#include <torch/types.h>

#include <cstddef>
#include <vector>

namespace radish {
using  Tensor= torch::Tensor;
/// Options for the `Embedding` module.
struct TORCH_API EmbeddingOptions {
  EmbeddingOptions(int64_t num_embeddings, int64_t embedding_dim) :
   num_embeddings_(num_embeddings), embedding_dim_(embedding_dim) {};
  /// The size of the dictionary of embeddings.
  TORCH_ARG(int64_t, num_embeddings);
  /// The size of each embedding vector.
  TORCH_ARG(int64_t, embedding_dim);
  /// If given, pads the output with the embedding vector at `padding_idx` (initialized to zeros) whenever it encounters the index.
  TORCH_ARG(c10::optional<int64_t>, padding_idx) = c10::nullopt;
  /// If given, each embedding vector with norm larger than `max_norm` is renormalized to have norm `max_norm`.
  TORCH_ARG(c10::optional<float>, max_norm) = c10::nullopt;
  /// The p of the p-norm to compute for the `max_norm` option. Default ``2``.
  TORCH_ARG(float, norm_type) = 2.;
  /// If given, this will scale gradients by the inverse of frequency of the words in the mini-batch. Default ``False``.
  TORCH_ARG(bool, scale_grad_by_freq) = false;
  /// If ``True``, gradient w.r.t. `weight` matrix will be a sparse tensor.
  TORCH_ARG(bool, sparse) = false;
  /// The learnable weights of the module of shape (num_embeddings, embedding_dim)
  TORCH_ARG(torch::Tensor, _weight) = Tensor();
};

/// Performs a lookup in a fixed size embedding table.
class TORCH_API EmbeddingImpl : public torch::nn::Cloneable<EmbeddingImpl> {
 public:
  EmbeddingImpl(int64_t num_embeddings, int64_t embedding_dim)
     : EmbeddingImpl(EmbeddingOptions(num_embeddings, embedding_dim)) {}
  explicit EmbeddingImpl(const EmbeddingOptions& options_);

  void reset() override;

  /// Pretty prints the `Embedding` module into the given `stream`.
  void pretty_print(std::ostream& stream) const override;

  /// Performs a lookup on the embedding table stored in `weight` using the
  /// `indices` supplied and returns the result.
  Tensor forward(const Tensor& indices);

  /// The `Options` used to configure this `Embedding` module.
  /// Changes to `EmbeddingOptions` *after construction* have no effect.
  EmbeddingOptions options;

  /// The embedding table.
  Tensor weight;
};

/// A `ModuleHolder` subclass for `EmbeddingImpl`.
/// See the documentation for `EmbeddingImpl` class to learn what methods it
/// provides, or the documentation for `ModuleHolder` to learn about PyTorch's
/// module storage semantics.
TORCH_MODULE(Embedding);

}  // namespace radish
