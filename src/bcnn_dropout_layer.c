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
#include "bcnn_dropout_layer.h"

#include <bh/bh_string.h>

#include "bcnn_utils.h"

bcnn_status bcnn_add_dropout_layer(bcnn_net *net, float rate, char *src_id) {
    int sz = 0, i;
    bcnn_node node = {0};

    BCNN_CHECK_AND_LOG(net->log_ctx, net->num_nodes >= 1,
                       BCNN_INVALID_PARAMETER,
                       "Dropout layer can't be the first layer of the network");

    int is_src_node_found = 0;
    for (i = net->num_tensors - 1; i >= 0; --i) {
        if (strcmp(net->tensors[i].name, src_id) == 0) {
            bcnn_node_add_input(net, &node, i);
            bcnn_node_add_output(net, &node, i);
            is_src_node_found = 1;
            break;
        }
    }
    BCNN_CHECK_AND_LOG(net->log_ctx, is_src_node_found, BCNN_INVALID_PARAMETER,
                       "Dropout layer: invalid input node name %s", src_id);

    node.layer = (bcnn_layer *)calloc(1, sizeof(bcnn_layer));
    node.layer->type = DROPOUT;
    node.layer->dropout_rate = rate;
    sz = bcnn_tensor_size(&net->tensors[node.src[0]]);
    node.layer->rand = (float *)calloc(sz, sizeof(float));
    node.layer->scale = 1.0f / (1.0f - rate);
#ifdef BCNN_USE_CUDA
    node.layer->rand_gpu = bcnn_cuda_memcpy_f32(node.layer->rand, sz);
#endif

    bcnn_net_add_node(net, node);

    BCNN_INFO(net->log_ctx,
              "[Dropout] input_shape= %dx%dx%d rate= %f output_shape= %dx%dx%d",
              net->tensors[node.src[0]].w, net->tensors[node.src[0]].h,
              net->tensors[node.src[0]].c, rate, net->tensors[node.dst[0]].w,
              net->tensors[node.dst[0]].h, net->tensors[node.dst[0]].c);
    return 0;
}

int bcnn_forward_dropout_layer_cpu(bcnn_layer *layer, bcnn_tensor *src_tensor,
                                   bcnn_tensor *dst_tensor, bcnn_state state) {
    int i, sz = bcnn_tensor_size(src_tensor);
    float r;

    if (state != TRAIN) {
        return BCNN_SUCCESS;
    }

    for (i = 0; i < sz; ++i) {
        r = (float)rand() / RAND_MAX;
        layer->rand[i] = r;
        if (r < layer->dropout_rate) {
            src_tensor->data[i] = 0;
        } else {
            src_tensor->data[i] *= layer->scale;
        }
    }
    return BCNN_SUCCESS;
}

int bcnn_forward_dropout_layer(bcnn_net *net, bcnn_node *node) {
    bcnn_tensor *src = &net->tensors[node->src[0]];
    bcnn_tensor *dst = &net->tensors[node->dst[0]];
#ifdef BCNN_USE_CUDA
    return bcnn_forward_dropout_layer_gpu(node->layer, src, dst, net->state);
#else
    return bcnn_forward_dropout_layer_cpu(node->layer, src, dst, net->state);
#endif
}

int bcnn_backward_dropout_layer_cpu(bcnn_layer *layer, bcnn_tensor *src_tensor,
                                    bcnn_tensor *dst_tensor) {
    int i, sz = bcnn_tensor_size(src_tensor);
    float r;

    if (!src_tensor->grad_data) {
        return BCNN_SUCCESS;
    }

    for (i = 0; i < sz; ++i) {
        r = layer->rand[i];
        if (r < layer->dropout_rate) {
            src_tensor->grad_data[i] = 0;
        } else {
            src_tensor->grad_data[i] *= layer->scale;
        }
    }
    return BCNN_SUCCESS;
}

int bcnn_backward_dropout_layer(bcnn_net *net, bcnn_node *node) {
    bcnn_tensor *src = &net->tensors[node->src[0]];
    bcnn_tensor *dst = &net->tensors[node->dst[0]];
#ifdef BCNN_USE_CUDA
    return bcnn_backward_dropout_layer_gpu(node->layer, src, dst);
#else
    return bcnn_backward_dropout_layer_cpu(node->layer, src, dst);
#endif
}