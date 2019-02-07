/*
 * Copyright (c) 2016-present Jean-Noel Braun.
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

#include "bcnn_classif_loader.h"

#include <bh/bh_macros.h>
#include <bh/bh_string.h>
#include "bcnn_data.h"
#include "bcnn_utils.h"

bcnn_status bcnn_loader_list_classif_init(bcnn_loader *iter, bcnn_net *net,
                                          const char *path_input,
                                          const char *path_extra) {
    FILE *f_list = NULL;
    f_list = fopen(path_input, "rb");
    if (f_list == NULL) {
        fprintf(stderr, "[ERROR] Can not open file %s\n", path_input);
        return BCNN_INVALID_PARAMETER;
    }
    // Allocate img buffer
    iter->input_uchar = (unsigned char *)calloc(
        net->tensors[0].w * net->tensors[0].h * net->tensors[0].c,
        sizeof(unsigned char));

    rewind(f_list);
    iter->f_input = f_list;
    return BCNN_SUCCESS;
}

void bcnn_loader_list_classif_terminate(bcnn_loader *iter) {
    if (iter->f_input != NULL) {
        fclose(iter->f_input);
    }
    bh_free(iter->input_uchar);
}

bcnn_status bcnn_loader_list_classif_next(bcnn_loader *iter, bcnn_net *net,
                                          int idx) {
    // nb_lines_skipped = (int)((float)rand() / RAND_MAX * net->batch_size);
    // bh_fskipline(f, nb_lines_skipped);
    char *line = bh_fgetline(iter->f_input);
    if (line == NULL) {
        rewind(iter->f_input);
        line = bh_fgetline(iter->f_input);
    }
    char **tok = NULL;
    int num_toks = bh_strsplit(line, ' ', &tok);
    if (net->mode != PREDICT) {
        BCNN_CHECK_AND_LOG(net->log_ctx, num_toks == 2, BCNN_INVALID_DATA,
                           "Wrong data format for classification");
    }
    // Load image, perform data augmentation if required and fill input tensor
    bcnn_fill_input_tensor(net, iter, tok[0], idx);
    // Fill label tensor (one-hot encoding)
    if (net->mode != PREDICT) {
        int label_sz = bcnn_tensor_size3d(&net->tensors[1]);
        float *y = net->tensors[1].data + idx * label_sz;
        memset(y, 0, label_sz * sizeof(float));
        y[atoi(tok[1])] = 1;
    }
    bh_free(line);
    for (int i = 0; i < num_toks; ++i) {
        bh_free(tok[i]);
    }
    bh_free(tok);

    return BCNN_SUCCESS;
}