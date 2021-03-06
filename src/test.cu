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

#include <fstream>
#include "marian.h"
#include "mnist.h"
#include "vocab.h"

#include "tensor_operators.h"

using namespace std;

///////////////////////////////////////////////////////
string output(const std::vector<float> &vec)
{
  stringstream strm;
  for (size_t i = 0; i < vec.size(); ++i) {
  strm << vec[i] << " ";
  }
  return strm.str();
}

//void testArgMax()
//{
//  using namespace std;
//  using namespace marian;
//
//  std::vector<float> hVec({29,19,  49,39,  79,99,  79,39});
//        cerr << "hVec =" << output(hVec) << endl;
//
//  thrust::device_vector<float> dVec(8);
//  thrust::copy(hVec.begin(), hVec.end(), dVec.begin());
//  float *data = thrust::raw_pointer_cast(dVec.data());
//
//  thrust::device_vector<float> dLabel(4);
//  float *labelPtr = thrust::raw_pointer_cast(dLabel.data());
//
//  gArgMax<<<4, 1, sizeof(float)>>>(labelPtr, data, 4, 2);
//
//  std::vector<float> hVec2(8);
//  thrust::copy(dVec.begin(), dVec.end(), hVec2.begin());
//  cerr << "hVec2=" << output(hVec2) << endl;
//
//  std::vector<float> hLabel(4);
//  thrust::copy(dLabel.begin(), dLabel.end(), hLabel.begin());
//  cerr << "hLabel=" << output(hLabel) << endl;
//
//  exit(0);
//}

///////////////////////////////////////////////////////
int main(int argc, char** argv) {
  //testArgMax();

  using namespace std;
  using namespace marian;
  using namespace keywords;

  Vocab sourceVocab, targetVocab;

  int input_size = 10;
  int output_size = 2;
  int batch_size = 25;
  int hidden_size = 5;
  int num_inputs = 8;

  std::vector<Expr> X;
  std::vector<Expr> Y;
  std::vector<Expr> H;

  ExpressionGraph g;

  for (int t = 0; t < num_inputs; ++t) {
    X.emplace_back(g.input(shape={batch_size, input_size}));
    Y.emplace_back(g.input(shape={batch_size, output_size}));
  }

  Expr Wxh = g.param(shape={input_size, hidden_size}, name="Wxh");
  Expr Whh = g.param(shape={hidden_size, hidden_size}, name="Whh");
  Expr bh = g.param(shape={1, hidden_size}, name="bh");
  Expr h0 = g.param(shape={1, hidden_size}, name="h0");

  // read parallel corpus from file
  std::fstream sourceFile("../examples/mt/dev/newstest2013.de");
  std::fstream targetFile("../examples/mt/dev/newstest2013.en");

  string sourceLine, targetLine;
  while (getline(sourceFile, sourceLine)) {
    getline(targetFile, targetLine);
    std::vector<size_t> sourceIds = sourceVocab.ProcessSentence(sourceLine);
    std::vector<size_t> targetIds = sourceVocab.ProcessSentence(targetLine);
  }

  std::cerr << "Building RNN..." << std::endl;
  H.emplace_back(tanh(dot(X[0], Wxh) + dot(h0, Whh) + bh));
  for (int t = 1; t < num_inputs; ++t) {
    H.emplace_back(tanh(dot(X[t], Wxh) + dot(H[t-1], Whh) + bh));
  }

  Expr Why = g.param(shape={hidden_size, output_size}, name="Why");
  Expr by = g.param(shape={1, output_size}, name="by");

  std::cerr << "Building output layer..." << std::endl;
  std::vector<Expr> Yp;

  Yp.emplace_back(softmax(dot(H[0], Why) + by));
  Expr cross_entropy = sum(Y[0] * log(Yp[0]), axis=1);
  for (int t = 1; t < num_inputs; ++t) {
    Yp.emplace_back(softmax(dot(H[t], Why) + by));
    cross_entropy = cross_entropy + sum(Y[t] * log(Yp[t]), axis=1);
  }
  auto graph = -mean(cross_entropy, axis=0, name="cost");

  for (int t = 0; t < num_inputs; ++t) {
    Tensor Xt({batch_size, input_size});
    Tensor Yt({batch_size, output_size});

    float max = 1.;
    std::vector<float> values(batch_size * input_size);
    std::vector<float> classes(batch_size * output_size, 0.0);
    int k = 0;
    int l = 0;
    for (int i = 0; i < batch_size; ++i) {
      for (int j = 0; j < input_size; ++j, ++k) {
         values[k] = max * (2.0*static_cast<float>(rand()) / RAND_MAX - 1.0);
      }
      int gold = output_size * static_cast<float>(rand()) / RAND_MAX;
      classes[l + gold] = 1.0;
      l += output_size;
    }

    thrust::copy(values.begin(), values.end(), Xt.begin());
    thrust::copy(classes.begin(), classes.end(), Yt.begin());

    X[t] = Xt;
    Y[t] = Yt;
  }

  std::cout << g.graphviz() << std::endl;
  
  g.forward(batch_size);
  g.backward();

  std::cerr << graph.val().Debug() << std::endl;

  std::cerr << X[0].val().Debug() << std::endl;
  std::cerr << Y[0].val().Debug() << std::endl;

  std::cerr << Whh.grad().Debug() << std::endl;
  std::cerr << bh.grad().Debug() << std::endl;
  std::cerr << Why.grad().Debug() << std::endl;
  std::cerr << by.grad().Debug() << std::endl;
  std::cerr << Wxh.grad().Debug() << std::endl;
  std::cerr << h0.grad().Debug() << std::endl;

  return 0;
}
