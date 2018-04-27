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
#include "bcnn_avgpool_layer.h"

#include <bh/bh.h>
#include <bh/bh_string.h>
#include "bcnn_utils.h"
#include "bh_log.h"

int bcnn_add_avgpool_layer(bcnn_net *net, int size, int stride, char *src_id,
                           char *dst_id) {
    int sz, i;
    bcnn_node node = {0};
    bcnn_tensor dst_tensor = {0};

    if (net->num_nodes > 0) {
        int is_src_node_found = 0;
        for (i = net->num_tensors - 1; i >= 0; --i) {
            if (strcmp(net->tensors[i].name, src_id) == 0) {
                bcnn_node_add_input(&node, i);
                is_src_node_found = 1;
                break;
            }
        }
        bh_check(is_src_node_found, "Avgpool layer: invalid input node name %s",
                 src_id);
    } else {
        bcnn_node_add_input(&node, 0);
    }

    bcnn_tensor_set_shape(&dst_tensor,
                          net->tensors[node.src[0]].n,  // batch size
                          net->tensors[node.src[0]].c,  // depth
                          1,                            // height
                          1,                            // width
                          1);
    bcnn_tensor_allocate(&dst_tensor);
    bh_strfill(&dst_tensor.name, dst_id);
    // Add node to net
    bcnn_net_add_tensor(net, dst_tensor);
    // Add tensor output index to node
    bcnn_node_add_output(&node, net->num_tensors - 1);

    node.layer = (bcnn_layer *)calloc(1, sizeof(bcnn_layer));
    node.layer->type = AVGPOOL;
    sz = bcnn_tensor_get_size(&net->tensors[node.dst[0]]);

    bcnn_net_add_node(net, node);

    bh_log_info(
        "[Avgpool] input_shape= %dx%dx%d ouput_shape= "
        "%dx%dx%d",
        net->tensors[node.src[0]].w, net->tensors[node.src[0]].h,
        net->tensors[node.src[0]].c, net->tensors[node.dst[0]].w,
        net->tensors[node.dst[0]].h, net->tensors[node.dst[0]].c);
    return 0;
}

int bcnn_forward_avgpool_layer_cpu(bcnn_layer *layer, bcnn_tensor *src_tensor,
                                   bcnn_tensor *dst_tensor) {
    for (int b = 0; b < src_tensor->n; ++b) {
        for (int k = 0; k < src_tensor->c; ++k) {
            int idx = k + b * src_tensor->c;
            dst_tensor->data[idx] = 0;
            for (int i = 0; i < src_tensor->h * src_tensor->w; ++i) {
                int offset =
                    src_tensor->h * src_tensor->w * (k + b * src_tensor->c) + i;
                dst_tensor->data[idx] += src_tensor->data[offset];
            }
            dst_tensor->data[idx] /= src_tensor->h * src_tensor->w;
        }
    }
    return BCNN_SUCCESS;
}

int bcnn_forward_avgpool_layer(bcnn_net *net, bcnn_node *node) {
    bcnn_tensor *src = &net->tensors[node->src[0]];
    bcnn_tensor *dst = &net->tensors[node->dst[0]];
#ifdef BCNN_USE_CUDA
    return bcnn_forward_avgpool_layer_gpu(node->layer, src, dst);
#else
    return bcnn_forward_avgpool_layer_cpu(node->layer, src, dst);
#endif
}

int bcnn_backward_avgpool_layer_cpu(bcnn_layer *layer, bcnn_tensor *src_tensor,
                                    bcnn_tensor *dst_tensor) {
    for (int b = 0; b < src_tensor->n; ++b) {
        for (int k = 0; k < src_tensor->c; ++k) {
            int idx = k + b * src_tensor->c;
            for (int i = 0; i < src_tensor->h * src_tensor->w; ++i) {
                int offset =
                    src_tensor->h * src_tensor->w * (k + b * src_tensor->c) + i;
                src_tensor->grad_data[offset] +=
                    dst_tensor->grad_data[idx] /
                    (src_tensor->h * src_tensor->w);
            }
        }
    }
    return BCNN_SUCCESS;
}

int bcnn_backward_avgpool_layer(bcnn_net *net, bcnn_node *node) {
    bcnn_tensor *src = &net->tensors[node->src[0]];
    bcnn_tensor *dst = &net->tensors[node->dst[0]];
#ifdef BCNN_USE_CUDA
    return bcnn_backward_avgpool_layer_gpu(node->layer, src, dst);
#else
    return bcnn_backward_avgpool_layer_cpu(node->layer, src, dst);
#endif
    return 0;
}
