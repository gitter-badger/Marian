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

#include "expression_operators.h"
#include "node_operators.h"

namespace marian {

Expr named(Expr a, const std::string& name) {
  a.graph()->add_named_node(a, name);
  return a;
}

Expr logit(Expr a) {
  return Expr(a.graph(), new LogitNodeOp(a));
}

Expr tanh(Expr a) {
  return Expr(a.graph(), new TanhNodeOp(a));
}

Expr relu(Expr a) {
  return Expr(a.graph(), new ReLUNodeOp(a));
}

Expr dropout(Expr a) {
  return Expr(a.graph(), new DropoutNodeOp(a));
}

Expr log(Expr a) {
  return Expr(a.graph(), new LogNodeOp(a));
};

Expr exp(Expr a) {
  return Expr(a.graph(), new ExpNodeOp(a));
};

Expr operator-(Expr a) {
  return Expr(a.graph(), new NegNodeOp(a));
};

Expr softmax(Expr a) {
  return Expr(a.graph(), new SoftmaxNodeOp(a));
}

Expr argmax(Expr a) {
  return Expr(a.graph(), new ArgmaxNodeOp(a));
}

/*********************************************************/

static Shape newShape(ChainPtr a, ChainPtr b) {
  size_t dimsA = a->shape().size();
  size_t dimsB = b->shape().size();
  UTIL_THROW_IF2(dimsA != dimsB,
                 "Tensors have different numbers of dimensions");
  Shape shape(dimsA);
  for(size_t i = 0; i < dimsA; ++i) {
    int dimA = a->shape()[i];
    int dimB = b->shape()[i];
    bool broadcastable = (dimA == dimB || dimA == 1 || dimB == 1);
    UTIL_THROW_IF2(!broadcastable, "Different dimensions in elementwise "
                   << "operation cannot be broadcasted: " << dimA << " != " << dimB);
    shape[i] = std::max(dimA, dimB);
    if(dimA == whatevs || dimB == whatevs)
      shape[i] = whatevs;
  }
  return shape;
}

Expr broadcast(Shape bShape, Expr a) {
  const Shape& aShape = a.node()->shape();
  if(aShape == bShape) {
    return a;
  }
  else {
    size_t dimsA = aShape.size();
    size_t dimsB = bShape.size();
    UTIL_THROW_IF2(dimsA != dimsB,
                   "Tensor and shape have different number of dimensions");
    for(size_t i = 0; i < dimsA; ++i) {
      int dimA = aShape[i];
      int dimB = bShape[i];
      bool broadcastable = (dimA == dimB || dimA == 1);
      UTIL_THROW_IF2(!broadcastable,
                     "Cannot broadcast tensor dimension "
                     << dimA << " to " << dimB);
      if(dimA == 1 && dimB != 1) {
        if(i == 0) {
          Expr one = a.graph()->ones(keywords::shape={bShape[0], 1});
          a = dot(one, a);
        }
        else if(i == 1) {
          Expr one = a.graph()->ones(keywords::shape={1, bShape[1]});
          a = dot(a, one);
        }
        else {
          UTIL_THROW2("Not implemented");        
        }
      }
    }
    return a;
  }
}

Expr operator+(Expr a, Expr b) {
  Shape shape = newShape(a, b);
  Expr cast_a = broadcast(shape, a);
  Expr cast_b = broadcast(shape, b);
  return Expr(a.graph(), new PlusNodeOp(cast_a, cast_b));
}

Expr operator-(Expr a, Expr b) {
  Shape shape = newShape(a, b);
  Expr cast_a = broadcast(shape, a);
  Expr cast_b = broadcast(shape, b);
  return Expr(a.graph(), new MinusNodeOp(cast_a, cast_b));
}

Expr operator*(Expr a, Expr b) {
  Shape shape = newShape(a, b);
  Expr cast_a = broadcast(shape, a);
  Expr cast_b = broadcast(shape, b);
  return Expr(a.graph(), new MultNodeOp(cast_a, cast_b));
}

Expr operator/(Expr a, Expr b) {
  Shape shape = newShape(a, b);
  Expr cast_a = broadcast(shape, a);
  Expr cast_b = broadcast(shape, b);
  return Expr(a.graph(), new DivNodeOp(cast_a, cast_b));
}

Expr dot(Expr a, Expr b) {
  return Expr(a.graph(), new DotNodeOp(a, b));
}

Expr cross_entropy(Expr a, Expr b) {
  return Expr(a.graph(), new CrossEntropyNodeOp(a, b));
}

}
