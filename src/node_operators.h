#pragma once

// This file is part of the Marian toolkit.
// Marian is copyright (c) 2016 Marcin Junczys-Dowmunt.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include "node.h"
#include "tensor_operators.h"

namespace marian {

struct InputNode : public Node {
  template <typename ...Args>
  InputNode(Args ...args)
  : Node(args...) {
    UTIL_THROW_IF2(!Has(keywords::shape) &&
                   !Has(keywords::lazy_shape),
                   "Data items require shape information");
  }

  virtual void setVal(Tensor t)  {
    val_ = t;
    shape_ = t.shape();
    //@todo, shape checking
  };

  void forward() {}
  void backward() {}
  
  virtual std::string graphviz() {
    std::stringstream ss;
    ss << "\"" << this << "\" [shape=\"parallelogram\", label=\"input\", style=\"filled\", fillcolor=\"lawngreen\"]" << std::endl << std::endl;
    return ss.str();
  };

};

struct ConstantNode : public Node {
  template <typename ...Args>
  ConstantNode(Args ...args)
  : Node(args...) {
    UTIL_THROW_IF2(!Has(keywords::shape) &&
                   !Has(keywords::lazy_shape),
                   "Constant items require shape information");
  }

  void forward() {}
  void backward() {}

  virtual std::string graphviz() {
    std::stringstream ss;
    ss << "\"" << this << "\" [shape=\"diamond\", label=\"const\"]" << std::endl << std::endl;
    return ss.str();
  };

};

struct ParamNode : public Node {
  template <typename ...Args>
  ParamNode(Args ...args)
  : Node(args...),
    init_(Get<std::function<void(Tensor)>>(keywords::init, [](Tensor){ })),
    initialized_(false)
  {
    UTIL_THROW_IF2(!Has(keywords::shape) &&
                   !Has(keywords::lazy_shape),
                   "Param items require shape information");
  }

  void forward() {}
  void backward() {}
  
  virtual void allocate(size_t batchSize) {
    val_.allocate(shape_);
    if(!initialized_) {
      init_(val_);
      initialized_ = true;
    }
  }

  virtual std::string graphviz() {
    std::stringstream ss;
    ss << "\"" << this << "\" [shape=\"hexagon\", label=\"param\", style=\"filled\", fillcolor=\"orangered\"]" << std::endl << std::endl;
    return ss.str();
  };

  
  private:
    std::function<void(Tensor)> init_;
    bool initialized_;
};

struct UnaryNodeOp : public Node {
    ChainPtr a_;

    template <typename ...Args>
    UnaryNodeOp(ChainPtr a, Args ...args)
    : Node(keywords::shape=a->shape(), //@TODO: Check keywords?
           args...), a_(a) {}
};

struct LogitNodeOp : public UnaryNodeOp {
  template <typename ...Args>
  LogitNodeOp(Args ...args)
  : UnaryNodeOp(args...) {  }

  void forward() {
    Element(_1 = Sigma(_2),
            val_, a_->val());
  }

  void backward() {
    Element(_1 += _2 * _3 * (1.0f - _3),
            a_->grad(), adj_, val_);
  }

  void check() {
    
  }
  
  virtual std::string graphviz() {
    std::stringstream ss;
    ss << "\"" << this << "\" [shape=\"box\", label=\"logit\", style=\"filled\", fillcolor=\"yellow\"]" << std::endl;
    ss << "\"" << a_ << "\" -> \"" << this << "\"" << std::endl << std::endl;
    return ss.str();
  };

};

struct TanhNodeOp : public UnaryNodeOp {
  template <typename ...Args>
  TanhNodeOp(Args ...args)
  : UnaryNodeOp(args...) { }

  void forward() {
    Element(_1 = Tanh(_2),
            val_, a_->val());
  }

  void backward() {
    Element(_1 += _2 * (1.0f - (_3 * _3)),
            a_->grad(), adj_, val_);
  }

  virtual std::string graphviz() {
    std::stringstream ss;
    ss << "\"" << this << "\" [shape=\"box\", label=\"tanh\", style=\"filled\", fillcolor=\"yellow\"]" << std::endl;
    ss << "\"" << a_ << "\" -> \"" << this << "\"" << std::endl << std::endl;
    return ss.str();
  };

};

struct ReLUNodeOp : public UnaryNodeOp {
  template <typename ...Args>
  ReLUNodeOp(Args ...args)
  : UnaryNodeOp(args...) { }

  void forward() {
    Element(_1 = Max(0.0f * _2, _2), // @TODO: fix 0 * _2
            val_, a_->val());
  }

  void backward() {
    Element(_1 += _2 * (_3 > 0.0f),
            a_->grad(), adj_, val_);
  }

  virtual std::string graphviz() {
    std::stringstream ss;
    ss << "\"" << this << "\" [shape=\"box\", label=\"ReLU\", style=\"filled\", fillcolor=\"yellow\"]" << std::endl;
    ss << "\"" << a_ << "\" -> \"" << this << "\"" << std::endl << std::endl;
    return ss.str();
  };

};

struct DropoutNodeOp : public UnaryNodeOp {
  template <typename ...Args>
  DropoutNodeOp(Args ...args)
  : UnaryNodeOp(args...),
    p_(0.5), seed_(time(0)) { }

  void forward() {
    Dropout(val_, a_->val(), p_, seed_++);
  }

  void backward() {
    Element(_1 += _2 * (_3 != 0.0f), // transform non-zero to 1 
            a_->grad(), adj_, val_);
  }

  virtual std::string graphviz() {
    std::stringstream ss;
    ss << "\"" << this << "\" [shape=\"box\", label=\"Dropout(" << p_ << ")\", style=\"filled\", fillcolor=\"yellow\"]" << std::endl;
    ss << "\"" << a_ << "\" -> \"" << this << "\"" << std::endl << std::endl;
    return ss.str();
  };

  private:
    float p_;
    int seed_;
};


struct SoftmaxNodeOp : public UnaryNodeOp {
  template <typename ...Args>
    SoftmaxNodeOp(Args ...args)
    : UnaryNodeOp(args...) { }

  void forward() {
    // B = softmax(A).
    thrust::copy(a_->val().begin(), a_->val().end(), val_.begin());
    // Safe version of softmax.
    Softmax(&val_);
  }

  void backward() {
    // For each row, the Jacobian times vector is given by:
    // J * dy = p .* (dy - avg*1)
    // where avg = p'*dy and p is the softmax output (probabilities).
    //
    // For more information, see sec. 2.5 of the following reference:
    // André F. T. Martins and Ramon Astudillo.
    // "From Softmax to Sparsemax: A Sparse Model of Attention and Multi-Label
    // Classification." ICML 2016.
    // http://jmlr.org/proceedings/papers/v48/martins16.pdf

    SoftmaxGrad(a_->grad(), adj_, val_);
  }

  virtual std::string graphviz() {
    std::stringstream ss;
    ss << "\"" << this << "\" [shape=\"box\", label=\"softmax\", style=\"filled\", fillcolor=\"yellow\"]" << std::endl;
    ss << "\"" << a_ << "\" -> \"" << this << "\"" << std::endl << std::endl;
    return ss.str();
  };
};

struct ArgmaxNodeOp : public UnaryNodeOp {
  template <typename ...Args>
  ArgmaxNodeOp(ChainPtr a, Args ...args)
    : UnaryNodeOp(a, keywords::shape=newShape(a), args...) { }

  void forward() {
    // B = softmax(A).
    Argmax(&val_, &a_->val());
  }

  void backward() {
  }

  Shape newShape(ChainPtr a) {
    Shape shape = a->shape();
    shape[1] = 1;
    return shape;
  }


  virtual std::string graphviz() {
    std::stringstream ss;
    ss << "\"" << this << "\" [shape=\"box\", label=\"argmax\", style=\"filled\", fillcolor=\"yellow\"]" << std::endl;
    ss << "\"" << a_ << "\" -> \"" << this << "\"" << std::endl << std::endl;
    return ss.str();
  };

};

struct LogNodeOp : public UnaryNodeOp {
  template <typename ...Args>
  LogNodeOp(Args ...args)
  : UnaryNodeOp(args...) {}

  void forward() {
    Element(_1 = Log(_2), val_, a_->val());
  }

  void backward() {
    Element(_1 += _2 * (1.f / _3),
            a_->grad(), adj_, a_->val());
  }
  
  virtual std::string graphviz() {
    std::stringstream ss;
    ss << "\"" << this << "\" [shape=\"box\", label=\"log\", style=\"filled\", fillcolor=\"yellow\"]" << std::endl;
    ss << "\"" << a_ << "\" -> \"" << this << "\"" << std::endl << std::endl;
    return ss.str();
  };

};

struct ExpNodeOp : public UnaryNodeOp {
  template <typename ...Args>
    ExpNodeOp(Args ...args)
    : UnaryNodeOp(args...) { }

  void forward() {
    Element(_1 = Exp(_2), val_, a_->val());
  }

  void backward() {
    Element(_1 += _2 * Exp(_3),
            a_->grad(), adj_, a_->val());
  }
  
  virtual std::string graphviz() {
    std::stringstream ss;
    ss << "\"" << this << "\" [shape=\"box\", label=\"exp\", style=\"filled\", fillcolor=\"yellow\"]" << std::endl;
    ss << "\"" << a_ << "\" -> \"" << this << "\"" << std::endl << std::endl;
    return ss.str();
  };

};

struct NegNodeOp : public UnaryNodeOp {
  template <typename ...Args>
  NegNodeOp(Args ...args)
  : UnaryNodeOp(args...) { }

  void forward() {
    Element(_1 = -_2, val_, a_->val());
  }

  void backward() {
    Element(_1 += -_2, a_->grad(), adj_);
  }
  
  virtual std::string graphviz() {
    std::stringstream ss;
    ss << "\"" << this << "\" [shape=\"box\", label=\"-\", style=\"filled\", fillcolor=\"yellow\"]" << std::endl;
    ss << "\"" << a_ << "\" -> \"" << this << "\"" << std::endl << std::endl;
    return ss.str();
  };

};

/******************************************************/

struct BinaryNodeOp : public Node {
  ChainPtr a_;
  ChainPtr b_;

  template <typename ...Args>
  BinaryNodeOp(ChainPtr a, ChainPtr b, Args ...args)
   : Node(args...), a_(a), b_(b) {}
};

/*** Matrix Product ***/

struct DotNodeOp : public BinaryNodeOp {
  template <typename ...Args>
  DotNodeOp(ChainPtr a, ChainPtr b, Args ...args)
  : BinaryNodeOp(a, b,
                 keywords::shape=newShape(a, b),
                 args...) { }

  Shape newShape(ChainPtr a, ChainPtr b) {
    Shape shape1 = a->shape();
    Shape shape2 = b->shape();
    UTIL_THROW_IF2(shape1[1] != shape2[0],
                   "matrix product requires dimensions to match");
    shape1[1] = shape2[1];
    return shape1;
  }

  void forward() {
    // C = A*B
    Prod(val_, a_->val(), b_->val(), false, false);
  }

  void backward() {
    // D is the adjoint, the matrix of derivatives
    // df/dA += D*B.T
    // df/dB += A.T*D
    // beta set to 1.0 in gemm, C = dot(A,B) + beta * C
    // to sum gradients from different graph parts
    Prod(a_->grad(), adj_, b_->val(), false, true, 1.0);
    Prod(b_->grad(), a_->val(), adj_, true, false, 1.0);
  }
  
  virtual std::string graphviz() {
    std::stringstream ss;
    ss << "\"" << this << "\" [shape=\"box\", label=\"×\", style=\"filled\", fillcolor=\"orange\"]" << std::endl;
    ss << "\"" << a_ << "\" -> \"" << this << "\"" << std::endl;
    ss << "\"" << b_ << "\" -> \"" << this << "\"" << std::endl << std::endl;
    return ss.str();
  };

};

struct PlusNodeOp : public BinaryNodeOp {
  template <typename ...Args>
  PlusNodeOp(ChainPtr a, ChainPtr b, Args ...args)
    : BinaryNodeOp(a, b, keywords::shape=a->shape(), args...) { }
    
  void forward() {
    Element(_1 = _2 + _3,
            val_, a_->val(), b_->val());
  }
  
  void backward() {
    Element(_1 += _2,
            a_->grad(), adj_);
    Element(_1 += _2,
            b_->grad(), adj_);
  }

  virtual std::string graphviz() {
    std::stringstream ss;
    ss << "\"" << this << "\" [shape=\"box\", label=\"+\", style=\"filled\", fillcolor=\"yellow\"]" << std::endl;
    ss << "\"" << a_ << "\" -> \"" << this << "\"" << std::endl;
    ss << "\"" << b_ << "\" -> \"" << this << "\"" << std::endl << std::endl;
    return ss.str();
  };

};

struct MinusNodeOp : public BinaryNodeOp {
  template <typename ...Args>
  MinusNodeOp(ChainPtr a, ChainPtr b, Args ...args)
    : BinaryNodeOp(a, b, keywords::shape=a->shape(), args...) { }
    
  void forward() {
    Element(_1 = _2 - _3,
            val_, a_->val(), b_->val());
  }
  
  void backward() {
    Element(_1 += _2,
            a_->grad(), adj_);
    Element(_1 -= _2,
            b_->grad(), adj_);
  }
  
  virtual std::string graphviz() {
    std::stringstream ss;
    ss << "\"" << this << "\" [shape=\"box\", label=\"-\", style=\"filled\", fillcolor=\"yellow\"]" << std::endl;
    ss << "\"" << a_ << "\" -> \"" << this << "\"" << std::endl;
    ss << "\"" << b_ << "\" -> \"" << this << "\"" << std::endl << std::endl;
    return ss.str();
  };

};

struct MultNodeOp : public BinaryNodeOp {
  template <typename ...Args>
  MultNodeOp(ChainPtr a, ChainPtr b, Args ...args)
    : BinaryNodeOp(a, b, keywords::shape=a->shape(), args...) { }
    
  void forward() {
    Element(_1 = _2 * _3,
            val_, a_->val(), b_->val());
  }
  
  void backward() {
    Element(_1 += _2 * _3,
            a_->grad(), adj_, b_->val());
    Element(_1 += _2 * _3,
            b_->grad(), adj_, a_->val());
  }
  
  virtual std::string graphviz() {
    std::stringstream ss;
    ss << "\"" << this << "\" [shape=\"box\", label=\"•\", style=\"filled\", fillcolor=\"yellow\"]" << std::endl;
    ss << "\"" << a_ << "\" -> \"" << this << "\"" << std::endl;
    ss << "\"" << b_ << "\" -> \"" << this << "\"" << std::endl << std::endl;
    return ss.str();
  };

};

struct DivNodeOp : public BinaryNodeOp {
  template <typename ...Args>
  DivNodeOp(ChainPtr a, ChainPtr b, Args ...args)
    : BinaryNodeOp(a, b, keywords::shape=a->shape(), args...) { }
    
  void forward() {
    Element(_1 = _2 / _3,
            val_, a_->val(), b_->val());
  }
  
  void backward() {
    Element(_1 += _2 * 1.0f / _3,
            a_->grad(), adj_, b_->val());
    Element(_1 -= _2 * _3 / (_4 * _4),
            b_->grad(), adj_, a_->val(), b_->val());
  }
  
  virtual std::string graphviz() {
    std::stringstream ss;
    ss << "\"" << this << "\" [shape=\"box\", label=\"÷\", style=\"filled\", fillcolor=\"yellow\"]" << std::endl;
    ss << "\"" << a_ << "\" -> \"" << this << "\"" << std::endl;
    ss << "\"" << b_ << "\" -> \"" << this << "\"" << std::endl << std::endl;
    return ss.str();
  };

};

// Cross-entropy node. It computes -b*log(softmax(a)), summing rowwise.
struct CrossEntropyNodeOp : public BinaryNodeOp {
  template <typename ...Args>
    CrossEntropyNodeOp(ChainPtr a, ChainPtr b, Args ...args)
    : BinaryNodeOp(a, b,
                   keywords::shape=newShape(a, b),
                   args...) { }

  Shape newShape(ChainPtr a, ChainPtr b) {
    Shape shape1 = a->shape();
    Shape shape2 = b->shape();
    UTIL_THROW_IF2(shape1[0] != shape2[0] || shape1[1] != shape2[1],
                   "cross entropy requires dimensions to match");
    shape1[1] = 1;
    return shape1;
  }

  // We're caching the softmax probabilities here because we'll need them for
  // the backward computation.
  void forward() {
    // C = -dot(B, log(softmax(A))).
    if (probs_) {
      probs_.set(0.0);
    } else {
      probs_.allocate(a_->val().shape(), 0.0);
    }
    thrust::copy(a_->val().begin(), a_->val().end(), probs_.begin());
    Softmax(&probs_); // Safe version of softmax.
    Tensor result(a_->val().shape());
    Element(_1 = -_2 * Log(_3), result, b_->val(), probs_);
    SumRowwise(result, val_);
  }

  // @TODO: In most cases it's wasteful to compute the derivative with respect
  // to the second input which is typically an input node in the computation
  // graph. In general the backward functions can skip the computation of
  // gradients wrt input nodes.
  void backward() {
    // For each row, the first input derivative is given by adj * (p - y),
    // where y is the gold label distribution (e.g. one hot vector) and
    // p is the softmax output (probabilities).
    // The second input derivative is -adj*log(p).
    Tensor result(probs_.shape());

    // Compute first input derivative.
    Element(_1 = _2 -  _3, result, probs_, b_->val());
    ScaleRowwise(result, adj_);
    Element(_1 += _2, a_->grad(), result);

    // Compute second input derivative.
    Element(_1 = -Log(_2), result, probs_); // @TODO: use a cached log here.
    ScaleRowwise(result, adj_);
    Element(_1 += _2, b_->grad(), result);
  }

  virtual std::string graphviz() {
    std::stringstream ss;
    ss << "\"" << this << "\" [shape=\"box\", label=\"cross_entropy\", style=\"filled\", fillcolor=\"yellow\"]" << std::endl;
    ss << "\"" << a_ << "\" -> \"" << this << "\"" << std::endl;
    ss << "\"" << b_ << "\" -> \"" << this << "\"" << std::endl << std::endl;
    return ss.str();
  };

 protected:
  Tensor probs_;

};

}
