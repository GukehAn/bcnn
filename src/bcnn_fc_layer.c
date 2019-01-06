/*
 * Copyright (c) 2016 Jean-Noel Braun.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
#include "bcnn_fc_layer.h"

#ifdef BCNN_USE_BLAS
#include "cblas.h"
#endif

#include <bh/bh_string.h>

#include "bcnn_activation_layer.h"
#include "bcnn_mat.h"
#include "bcnn_utils.h"

bcnn_status bcnn_add_fullc_layer(bcnn_net *net, int output_size,
                                 bcnn_filler_type init,
                                 bcnn_activation activation, int quantize,
                                 char *src_id, char *dst_id) {
    int i;
    bcnn_node node = {0};
    bcnn_tensor dst_tensor = {0};

    if (net->num_nodes > 0) {
        int is_src_node_found = 0;
        for (i = net->num_tensors - 1; i >= 0; --i) {
            if (strcmp(net->tensors[i].name, src_id) == 0) {
                bcnn_node_add_input(net, &node, i);
                is_src_node_found = 1;
                break;
            }
        }
        BCNN_CHECK_AND_LOG(
            net->log_ctx, is_src_node_found, BCNN_INVALID_PARAMETER,
            "Full-connected layer: invalid input node name %s", src_id);
    } else {
        bcnn_node_add_input(net, &node, 0);
    }

    node.layer = (bcnn_layer *)calloc(1, sizeof(bcnn_layer));
    node.layer->type = FULL_CONNECTED;
    node.layer->gemm_ctx = net->gemm_ctx;

    int input_size = bcnn_tensor_size3d(&net->tensors[node.src[0]]);

    // Setup weights and biases
    char weights_name[256];
    sprintf(weights_name, "%s_w", src_id);
    char biases_name[256];
    sprintf(biases_name, "%s_b", src_id);
    // Create weights tensor
    bcnn_tensor weights = {0};
    bcnn_tensor_create(&weights, output_size, net->tensors[node.src[0]].c,
                       net->tensors[node.src[0]].h, net->tensors[node.src[0]].w,
                       1, weights_name, net->state);
    bcnn_tensor_filler w_filler = {.range = input_size, .type = init};
    bcnn_tensor_fill(&weights, w_filler);
    bcnn_net_add_tensor(net, weights);
    bcnn_node_add_input(net, &node, net->num_tensors - 1);
    // Create bias tensor
    bcnn_tensor biases = {0};
    bcnn_tensor_create(&biases, 1, 1, 1, output_size, 1, biases_name,
                       net->state);
    bcnn_net_add_tensor(net, biases);
    bcnn_node_add_input(net, &node, net->num_tensors - 1);

    // Setup output tensor
    bcnn_tensor_set_shape(&dst_tensor,
                          net->tensors[node.src[0]].n,  // batch size
                          output_size,                  // depth
                          1,                            // height
                          1,                            // width
                          1);
    bcnn_tensor_allocate(&dst_tensor, net->state);
    bh_strfill(&dst_tensor.name, dst_id);
    // Add tensor to net
    bcnn_net_add_tensor(net, dst_tensor);
    // Add tensor output index to node
    bcnn_node_add_output(net, &node, net->num_tensors - 1);

    if (net->learner.optimizer == ADAM) {
        int weights_size = bcnn_tensor_size(&weights);
        node.layer->adam_m = (float *)calloc(weights_size, sizeof(float));
        node.layer->adam_v = (float *)calloc(weights_size, sizeof(float));
    }

#ifdef BCNN_USE_CUDA
    if (net->learner.optimizer == ADAM) {
        int weights_size = bcnn_tensor_size(&weights);
        node.layer->adam_m_gpu =
            bcnn_cuda_memcpy_f32(node.layer->adam_m, weights_size);
        node.layer->adam_v_gpu =
            bcnn_cuda_memcpy_f32(node.layer->adam_v, weights_size);
    }
#endif
    node.layer->activation = activation;

    node.type = FULL_CONNECTED;
    node.param_size = sizeof(bcnn_fullc_param);
    node.param = (bcnn_fullc_param *)calloc(1, node.param_size);
    bcnn_fullc_param *param = (bcnn_fullc_param *)node.param;
    if (net->learner.optimizer == ADAM) {
        int weights_size = bcnn_tensor_size(&weights);
        param->adam_m = (float *)calloc(weights_size, sizeof(float));
        param->adam_v = (float *)calloc(weights_size, sizeof(float));
    }
#ifdef BCNN_USE_CUDA
    if (net->learner.optimizer == ADAM) {
        int weights_size = bcnn_tensor_size(&weights);
        param->adam_m_gpu = bcnn_cuda_memcpy_f32(param->adam_m, weights_size);
        param->adam_v_gpu = bcnn_cuda_memcpy_f32(param->adam_v, weights_size);
    }
#endif
    node.forward = bcnn_forward_fullc_layer;
    node.backward = bcnn_backward_fullc_layer;

    bcnn_net_add_node(net, node);

    BCNN_INFO(net->log_ctx,
              "[Connected] input_shape= %dx%dx%d output_shape= %dx%dx%d",
              net->tensors[node.src[0]].w, net->tensors[node.src[0]].h,
              net->tensors[node.src[0]].c, net->tensors[node.dst[0]].w,
              net->tensors[node.dst[0]].h, net->tensors[node.dst[0]].c);

    return BCNN_SUCCESS;
}

void bcnn_forward_fullc_layer_cpu(bcnn_net *net, bcnn_node *node) {
    bcnn_tensor *src_tensor = &net->tensors[node->src[0]];
    bcnn_tensor *dst_tensor = &net->tensors[node->dst[0]];
    bcnn_tensor *weights = &net->tensors[node->src[1]];
    bcnn_tensor *biases = &net->tensors[node->src[2]];
    bcnn_fullc_param *param = (bcnn_fullc_param *)node->param;
    int batch_size = dst_tensor->n;
    int src_size = bcnn_tensor_size3d(src_tensor);
    int dst_size = bcnn_tensor_size3d(dst_tensor);
    int sz = bcnn_tensor_size(dst_tensor);

    memset(dst_tensor->data, 0, dst_size * batch_size * sizeof(float));

#ifdef BCNN_USE_BLAS
    cblas_sgemm(CblasRowMajor, CblasNoTrans, CblasTrans, batch_size, dst_size,
                src_size, 1.0f, src_tensor->data, src_size, weights->data,
                src_size, 1.0f, dst_tensor->data, dst_size);
#else
    // Original
    bcnn_gemm(net->gemm_ctx, 0, 1, batch_size, dst_size, src_size, 1.0f,
              src_tensor->data, src_size, weights->data, src_size, 1.0f,
              dst_tensor->data, dst_size);
#endif

    for (int i = 0; i < batch_size; ++i) {
        bcnn_axpy(dst_size, 1, biases->data, dst_tensor->data + i * dst_size);
    }

    bcnn_forward_activation_cpu(dst_tensor->data, sz, param->activation);

    return;
}

void bcnn_backward_fullc_layer_cpu(bcnn_net *net, bcnn_node *node) {
    bcnn_tensor *src_tensor = &net->tensors[node->src[0]];
    bcnn_tensor *dst_tensor = &net->tensors[node->dst[0]];
    bcnn_tensor *weights = &net->tensors[node->src[1]];
    bcnn_tensor *biases = &net->tensors[node->src[2]];
    bcnn_fullc_param *param = (bcnn_fullc_param *)node->param;
    int batch_size = dst_tensor->n;
    int src_size = bcnn_tensor_size3d(src_tensor);
    int dst_size = bcnn_tensor_size3d(dst_tensor);
    int sz = bcnn_tensor_size(dst_tensor);

    bcnn_backward_activation_cpu(dst_tensor->data, dst_tensor->grad_data, sz,
                                 param->activation);

    for (int i = 0; i < batch_size; ++i) {
        bcnn_axpy(dst_size, 1, dst_tensor->grad_data + i * dst_size,
                  biases->grad_data);
    }

#ifdef BCNN_USE_BLAS
    cblas_sgemm(CblasRowMajor, CblasTrans, CblasNoTrans, dst_size, src_size,
                batch_size, 1.0f, dst_tensor->grad_data, dst_size,
                src_tensor->data, src_size, 1.0f, weights->grad_data, src_size);
#else
    // Original
    bcnn_gemm(net->gemm_ctx, 1, 0, dst_size, src_size, batch_size, 1.0f,
              dst_tensor->grad_data, dst_size, src_tensor->data, src_size, 1.0f,
              weights->grad_data, src_size);
#endif

    if (src_tensor->grad_data) {
#ifdef BCNN_USE_BLAS
        cblas_sgemm(CblasRowMajor, CblasNoTrans, CblasNoTrans, batch_size,
                    src_size, dst_size, 1.0f, dst_tensor->grad_data, dst_size,
                    weights->data, src_size, 1.0f, src_tensor->grad_data,
                    src_size);
#else
        // Original
        bcnn_gemm(net->gemm_ctx, 0, 0, batch_size, src_size, dst_size, 1.0f,
                  dst_tensor->grad_data, dst_size, weights->data, src_size,
                  1.0f, src_tensor->grad_data, src_size);
#endif
    }

    return;
}

#ifdef BCNN_USE_CUDA
void bcnn_forward_fullc_layer_gpu(bcnn_net *net, bcnn_node *node) {
    bcnn_tensor *src_tensor = &net->tensors[node->src[0]];
    bcnn_tensor *dst_tensor = &net->tensors[node->dst[0]];
    bcnn_tensor *weights = &net->tensors[node->src[1]];
    bcnn_tensor *biases = &net->tensors[node->src[2]];
    bcnn_fullc_param *param = (bcnn_fullc_param *)node->param;
    int batch_size = dst_tensor->n;
    int src_size = bcnn_tensor_size3d(src_tensor);
    int dst_size = bcnn_tensor_size3d(dst_tensor);
    int sz = bcnn_tensor_size(dst_tensor);

    bcnn_cuda_fill_f32(dst_size * batch_size, 0.0f, dst_tensor->data_gpu, 1);

    bcnn_cuda_gemm(0, 1, batch_size, dst_size, src_size, 1,
                   src_tensor->data_gpu, src_size, weights->data_gpu, src_size,
                   1, dst_tensor->data_gpu, dst_size);

    for (int i = 0; i < batch_size; ++i) {
        bcnn_cuda_axpy(dst_size, 1, biases->data_gpu, 1,
                       dst_tensor->data_gpu + i * dst_size, 1);
    }
    bcnn_forward_activation_gpu(dst_tensor->data_gpu, sz, param->activation);

    return;
}

void bcnn_backward_fullc_layer_gpu(bcnn_net *net, bcnn_node *node) {
    bcnn_tensor *src_tensor = &net->tensors[node->src[0]];
    bcnn_tensor *dst_tensor = &net->tensors[node->dst[0]];
    bcnn_tensor *weights = &net->tensors[node->src[1]];
    bcnn_tensor *biases = &net->tensors[node->src[2]];
    bcnn_fullc_param *param = (bcnn_fullc_param *)node->param;
    int batch_size = dst_tensor->n;
    int src_size = bcnn_tensor_size3d(src_tensor);
    int dst_size = bcnn_tensor_size3d(dst_tensor);
    int sz = bcnn_tensor_size(dst_tensor);

    bcnn_backward_activation_gpu(
        dst_tensor->data_gpu, dst_tensor->grad_data_gpu, sz, param->activation);

    for (int i = 0; i < batch_size; ++i) {
        bcnn_cuda_axpy(dst_size, 1, dst_tensor->grad_data_gpu + i * dst_size, 1,
                       biases->grad_data_gpu, 1);
    }

    bcnn_cuda_gemm(1, 0, dst_size, src_size, batch_size, 1,
                   dst_tensor->grad_data_gpu, dst_size, src_tensor->data_gpu,
                   src_size, 1, weights->grad_data_gpu, src_size);
    if (src_tensor->grad_data_gpu) {
        bcnn_cuda_gemm(0, 0, batch_size, src_size, dst_size, 1,
                       dst_tensor->grad_data_gpu, dst_size, weights->data_gpu,
                       src_size, 1, src_tensor->grad_data_gpu, src_size);
    }

    return;
}
#endif

void bcnn_forward_fullc_layer(bcnn_net *net, bcnn_node *node) {
#ifdef BCNN_USE_CUDA
    return bcnn_forward_fullc_layer_gpu(net, node);
#else
    return bcnn_forward_fullc_layer_cpu(net, node);
#endif
}

void bcnn_backward_fullc_layer(bcnn_net *net, bcnn_node *node) {
#ifdef BCNN_USE_CUDA
    return bcnn_backward_fullc_layer_gpu(net, node);
#else
    return bcnn_backward_fullc_layer_cpu(net, node);
#endif
}