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

#include "bcnn_data.h"

#include <bh/bh_log.h>
#include <bh/bh_macros.h>
#include <bh/bh_string.h>

/* include bip image processing lib */
#include <bip/bip.h>

#include "bcnn/bcnn.h"
#include "bcnn_net.h"
#include "bcnn_tensor.h"
#include "bcnn_utils.h"
#include "data_loader/bcnn_cifar10_loader.h"
#include "data_loader/bcnn_classif_loader.h"
#include "data_loader/bcnn_detection_loader.h"
#include "data_loader/bcnn_mnist_loader.h"
#include "data_loader/bcnn_regression_loader.h"

void bcnn_convert_img_to_float(unsigned char *src, int w, int h, int c,
                               float norm_coeff, int swap_to_bgr, float mean_r,
                               float mean_g, float mean_b, float *dst) {
    float m[3] = {mean_r, mean_g, mean_b};
    if (swap_to_bgr) {
        if (c != 3) {
            bh_log(BH_LOG_ERROR,
                   "bcnn_convert_img_to_float: number of channels %d is "
                   "inconsistent. Expected 3",
                   c);
            return;
        }
        for (int k = 0; k < c; ++k) {
            for (int y = 0; y < h; ++y) {
                for (int x = 0; x < w; ++x) {
                    dst[w * (h * (2 - k) + y) + x] =
                        ((float)src[c * (x + w * y) + k] - m[k]) * norm_coeff;
                }
            }
        }
    } else {
        for (int k = 0; k < c; ++k) {
            for (int y = 0; y < h; ++y) {
                for (int x = 0; x < w; ++x) {
                    dst[w * (h * k + y) + x] =
                        ((float)src[c * (x + w * y) + k] - m[k]) * norm_coeff;
                }
            }
        }
    }
}

/* Load image from disk, performs crop to fit the required size if needed and
 * copy in pre-allocated memory */
static bcnn_status bcnn_load_image_from_path(bcnn_net *net, char *path, int w,
                                             int h, int c, unsigned char *img,
                                             int *x_shift, int *y_shift) {
    int w_img, h_img, c_img, x_ul = 0, y_ul = 0;
    unsigned char *buf = NULL, *pimg = NULL;

    bip_load_image(path, &buf, &w_img, &h_img, &c_img);
    BCNN_CHECK_AND_LOG(net->log_ctx, (w_img > 0 && h_img > 0 && buf),
                       BCNN_INVALID_DATA, "Invalid image %s", path);
    if (c != c_img) {
        bcnn_log(net->log_ctx, BCNN_LOG_ERROR,
                 "Unexpected number of channels of image %s\n", path);
        bh_free(buf);
        return BCNN_INVALID_DATA;
    }

    if (w_img != w || h_img != h) {
        if (net->mode == BCNN_MODE_PREDICT || net->mode == BCNN_MODE_VALID) {
            x_ul = (w_img - w) / 2;
            y_ul = (h_img - h) / 2;
        } else {  // mode train, random crop
            x_ul = rand_between(0, (w_img - w));
            y_ul = rand_between(0, (h_img - h));
        }
        pimg = (unsigned char *)calloc(w * h * c, sizeof(unsigned char));
        bip_crop_image(buf, w_img, h_img, w_img * c_img, x_ul, y_ul, pimg, w, h,
                       w * c, c);
        memcpy(img, pimg, w * h * c);
        bh_free(pimg);
    } else {
        memcpy(img, buf, w * h * c);
    }
    bh_free(buf);
    *x_shift = x_ul;
    *y_shift = y_ul;

    return BCNN_SUCCESS;
}

/* Data augmentation */
bcnn_status bcnn_data_augmentation(unsigned char *img, int width, int height,
                                   int depth, bcnn_data_augmenter *param,
                                   unsigned char *buffer) {
    int sz = width * height * depth;
    unsigned char *img_scale = NULL;
    int x_ul = 0, y_ul = 0, w_scale, h_scale;
    float scale = 1.0f, theta = 0.0f, contrast = 1.0f, kx, ky, distortion;
    int brightness = 0;

    if (param->random_fliph) {
        if (param->apply_fliph) {
            bip_fliph_image(img, width, height, depth, width * depth, buffer,
                            width * depth);
            memcpy(img, buffer, sz * sizeof(unsigned char));
        }
    }
    if (param->range_shift_x || param->range_shift_y) {
        memset(buffer, 128, sz);
        if (param->use_precomputed) {
            x_ul = param->shift_x;
            y_ul = param->shift_y;
        } else {
            x_ul = (int)((float)(rand() - RAND_MAX / 2) / RAND_MAX *
                         param->range_shift_x);
            y_ul = (int)((float)(rand() - RAND_MAX / 2) / RAND_MAX *
                         param->range_shift_y);
            param->shift_x = x_ul;
            param->shift_y = y_ul;
        }
        bip_crop_image(img, width, height, width * depth, x_ul, y_ul, buffer,
                       width, height, width * depth, depth);
        memcpy(img, buffer, sz * sizeof(unsigned char));
    }
    if (param->max_scale > 0.0f || param->min_scale > 0.0f) {
        if (param->use_precomputed) {
            scale = param->scale;
        } else {
            scale = (((float)rand() / RAND_MAX) *
                         (param->max_scale - param->min_scale) +
                     param->min_scale);
            param->scale = scale;
        }
        w_scale = (int)(width * scale);
        h_scale = (int)(height * scale);
        img_scale = (unsigned char *)calloc(w_scale * h_scale * depth,
                                            sizeof(unsigned char));
        bip_resize_bilinear(img, width, height, width * depth, img_scale,
                            w_scale, h_scale, w_scale * depth, depth);
        bip_crop_image(img_scale, w_scale, h_scale, w_scale * depth, x_ul, y_ul,
                       img, width, height, width * depth, depth);
        bh_free(img_scale);
    }
    if (param->rotation_range > 0.0f) {
        if (param->use_precomputed) {
            theta = param->rotation;
        } else {
            theta = bip_deg2rad((float)(rand() - RAND_MAX / 2) / RAND_MAX *
                                param->rotation_range);
            param->rotation = theta;
        }
        memset(buffer, 128, sz);
        bip_rotate_image(img, width, height, width * depth, buffer, width,
                         height, width * depth, depth, theta, width / 2,
                         height / 2, BILINEAR);
        memcpy(img, buffer, width * height * depth * sizeof(unsigned char));
    }
    if (param->min_contrast > 0.0f || param->max_contrast > 0.0f) {
        if (param->use_precomputed) {
            contrast = param->contrast;
        } else {
            contrast = (((float)rand() / RAND_MAX) *
                            (param->max_contrast - param->min_contrast) +
                        param->min_contrast);
            param->contrast = contrast;
        }
        bip_contrast_stretch(img, width * depth, width, height, depth, img,
                             width * depth, contrast);
    }
    if (param->min_brightness != 0 || param->max_brightness != 0) {
        if (param->use_precomputed) {
            brightness = param->brightness;
        } else {
            brightness =
                (int)(((float)rand() / RAND_MAX) *
                          (param->max_brightness - param->min_brightness) +
                      param->min_brightness);
            param->brightness = brightness;
        }
        bip_image_brightness(img, width * depth, width, height, depth, img,
                             width * depth, brightness);
    }
    if (param->max_distortion > 0.0f) {
        if (param->use_precomputed) {
            kx = param->distortion_kx;
            ky = param->distortion_ky;
            distortion = param->distortion;
        } else {
            kx = (((float)rand() - RAND_MAX / 2) / RAND_MAX);
            ky = (((float)rand() - RAND_MAX / 2) / RAND_MAX);
            distortion = ((float)rand() / RAND_MAX) * (param->max_distortion);
            param->distortion_kx = kx;
            param->distortion_ky = ky;
            param->distortion = distortion;
        }
        bip_image_perlin_distortion(img, width * depth, width, height, depth,
                                    buffer, width * depth, param->distortion,
                                    kx, ky);
        memcpy(img, buffer, width * height * depth * sizeof(unsigned char));
    }
    if (param->max_random_spots > 0) {
        int num_spots = rand_between(0, param->max_random_spots);
        bip_add_random_spotlights(img, width * depth, width, height, depth,
                                  buffer, width * depth, num_spots, 0.3f, 3.0f,
                                  0.3f, 3.0f);
        memcpy(img, buffer, width * height * depth * sizeof(unsigned char));
    }

    return BCNN_SUCCESS;
}

void bcnn_fill_input_tensor(bcnn_net *net, bcnn_loader *iter, char *path_img,
                            int idx) {
    bcnn_load_image_from_path(
        net, path_img, net->tensors[0].w, net->tensors[0].h, net->tensors[0].c,
        iter->input_uchar, &net->data_aug->shift_x, &net->data_aug->shift_y);
    // Data augmentation
    if (net->mode == BCNN_MODE_TRAIN) {
        int use_buffer_img = (net->data_aug->range_shift_x != 0 ||
                              net->data_aug->range_shift_y != 0 ||
                              net->data_aug->rotation_range != 0 ||
                              net->data_aug->random_fliph != 0);
        unsigned char *img_tmp = NULL;
        if (use_buffer_img) {
            int sz_img = bcnn_tensor_size3d(&net->tensors[0]);
            img_tmp = (unsigned char *)calloc(sz_img, sizeof(unsigned char));
        }
        bcnn_data_augmentation(iter->input_uchar, net->tensors[0].w,
                               net->tensors[0].h, net->tensors[0].c,
                               net->data_aug, img_tmp);
        bh_free(img_tmp);
    }
    // Fill input tensor
    int input_sz = bcnn_tensor_size3d(&net->tensors[0]);
    float *x = net->tensors[0].data + idx * input_sz;
    // Map [0;255] uint8 values to [-1;1] float values
    bcnn_convert_img_to_float(iter->input_uchar, net->tensors[0].w,
                              net->tensors[0].h, net->tensors[0].c, 1 / 127.5f,
                              net->data_aug->swap_to_bgr, 127.5f, 127.5f,
                              127.5f, x);
}

bcnn_loader_init_func bcnn_iterator_init_lut[BCNN_NUM_LOADERS] = {
    bcnn_loader_mnist_init, bcnn_loader_cifar10_init,
    bcnn_loader_list_classif_init, bcnn_loader_list_reg_init,
    bcnn_loader_list_detection_init};

bcnn_loader_next_func bcnn_iterator_next_lut[BCNN_NUM_LOADERS] = {
    bcnn_loader_mnist_next, bcnn_loader_cifar10_next,
    bcnn_loader_list_classif_next, bcnn_loader_list_reg_next,
    bcnn_loader_list_detection_next};

bcnn_loader_terminate_func bcnn_iterator_terminate_lut[BCNN_NUM_LOADERS] = {
    bcnn_loader_mnist_terminate, bcnn_loader_cifar10_terminate,
    bcnn_loader_list_classif_terminate, bcnn_loader_list_reg_terminate,
    bcnn_loader_list_detection_terminate};

bcnn_status bcnn_loader_initialize(bcnn_loader *iter, bcnn_loader_type type,
                                   bcnn_net *net, const char *train_path,
                                   const char *train_path_extra,
                                   const char *test_path,
                                   const char *test_path_extra) {
    iter->type = type;
    return bcnn_iterator_init_lut[iter->type](
        iter, net, train_path, train_path_extra, test_path, test_path_extra);
}

bcnn_status bcnn_loader_next(bcnn_net *net) {
    for (int i = 0; i < net->batch_size; ++i) {
        bcnn_loader *iter = net->data_loader;
        if (bcnn_iterator_next_lut[iter->type](iter, net, i) != BCNN_SUCCESS) {
            // Handle the case when one sample could not be loaded correctly for
            // some reason (wrong path, image corrupted ...): skip this sample
            // and try to load the next one.
            --i;
            continue;
        }
    }
#ifdef BCNN_USE_CUDA
    bcnn_cuda_memcpy_host2dev(net->tensors[0].data_gpu, net->tensors[0].data,
                              bcnn_tensor_size(&net->tensors[0]));
    if (net->mode != BCNN_MODE_PREDICT) {
        bcnn_cuda_memcpy_host2dev(net->tensors[1].data_gpu,
                                  net->tensors[1].data,
                                  bcnn_tensor_size(&net->tensors[1]));
    }
#endif
    return BCNN_SUCCESS;
}

void bcnn_loader_terminate(bcnn_loader *iter) {
    return bcnn_iterator_terminate_lut[iter->type](iter);
}

bcnn_status bcnn_set_data_loader(bcnn_net *net, bcnn_loader_type type,
                                 const char *train_path_data,
                                 const char *train_path_extra,
                                 const char *test_path_data,
                                 const char *test_path_extra) {
    if (net->data_loader != NULL) {
        bcnn_loader_terminate(net->data_loader);
        bh_free(net->data_loader);
    }
    net->loader = (bcnn_loader *)calloc(1, sizeof(bcnn_loader));
    if (net->loader == NULL) {
        return BCNN_FAILED_ALLOC;
    }
    return bcnn_loader_initialize(&net->loader, type, net, train_path_data,
                                  train_path_extra, test_path_data,
                                  test_path_extra);
}

void bcnn_destroy_data_loader(bcnn_net *net) {
    bcnn_loader_terminate(net->loader);
    bh_free(net->loader);
}

bcnn_status bcnn_set_data_augmentation(bcnn_net *net,
                                       bcnn_data_augment_param param) {
    if (net->data_aug != NULL) {
        bh_free(net->data_aug);
    }
    net->data_aug =
        (bcnn_data_augmenter *)calloc(1, sizeof(bcnn_data_augmenter));
    if (net->data_aug == NULL) {
        return BCNN_FAILED_ALLOC;
    }
    net->data_aug->range_shift_x = param.range_shift_x;
    net->data_aug->range_shift_y = param.range_shift_y;
    net->data_aug->random_fliph = param.random_fliph;
    net->data_aug->min_brightness = param.min_brightness;
    net->data_aug->max_brightness = param.max_brightness;
    net->data_aug->swap_to_bgr = param.swap_to_bgr;
    net->data_aug->no_input_norm = param.no_input_norm;
    net->data_aug->max_random_spots = param.max_random_spots;
    net->data_aug->min_scale = param.min_scale;
    net->data_aug->max_scale = param.max_scale;
    net->data_aug->rotation_range = param.rotation_range;
    net->data_aug->min_contrast = param.min_contrast;
    net->data_aug->max_contrast = param.max_contrast;
    net->data_aug->max_distortion = param.max_distortion;
    net->data_aug->mean_r = param.mean_r;
    net->data_aug->mean_g = param.mean_g;
    net->data_aug->mean_b = param.mean_b;

    return BCNN_SUCCESS;
}

bcnn_status bcnn_open_dataset(bcnn_loader *iter, bcnn_net *net,
                              const char *train_path,
                              const char *train_path_extra,
                              const char *test_path,
                              const char *test_path_extra, bool has_extra) {
    // Open the files handles according to each dataset path
    if (train_path_img != NULL) {
        iter->f_train = fopen(train_path_img, "rb");
        BCNN_CHECK_AND_LOG(net->log_ctx, iter->f_train, BCNN_INVALID_PARAMETER,
                           "Could not open file %s", train_path_img);
    }
    if (test_path_img != NULL) {
        iter->f_test = fopen(test_path_img, "rb");
        BCNN_CHECK_AND_LOG(net->log_ctx, iter->f_test, BCNN_INVALID_PARAMETER,
                           "Could not open file %s", test_path_img);
    }
    if (has_extra) {
        if (train_path_extra != NULL) {
            iter->f_train_extra = fopen(iter->f_train_extra, "rb");
            BCNN_CHECK_AND_LOG(net->log_ctx, iter->f_train_extra,
                               BCNN_INVALID_PARAMETER, "Could not open file %s",
                               train_path_extra);
        }
        if (test_path_extra != NULL) {
            iter->f_test_extra = fopen(test_path_extra, "rb");
            BCNN_CHECK_AND_LOG(net->log_ctx, iter->f_test_extra,
                               BCNN_INVALID_PARAMETER, "Could not open file %s",
                               test_path_extra);
        }
    }
    // Check that the provided dataset are consistent with the network mode
    if (net->mode == BCNN_MODE_TRAIN) {
        iter->f_current = iter->f_train;
        bool valid = (iter->f_current != NULL);
        if (has_extra) {
            iter->f_current_extra = iter->f_train_extra;
            valid = valid && (iter->f_current_extra != NULL);
        }
        BCNN_CHECK_AND_LOG(net->log_ctx, valid, BCNN_INVALID_DATA,
                           "A training dataset must be provided");
    } else {
        iter->f_current = iter->f_test;
        bool valid = (iter->f_current != NULL);
        if (has_extra) {
            iter->f_current_extra = iter->f_test_extra;
            valid = valid && (iter->f_current_extra != NULL);
        }
        BCNN_CHECK_AND_LOG(net->log_ctx, valid, BCNN_INVALID_DATA,
                           "A testing dataset must be provided");
    }
    iter->has_extra_data = has_extra;
    return BCNN_SUCCESS;
}

bcnn_status bcnn_switch_data_handles(bcnn_net *net, bcnn_loader *iter) {
    if (net->mode == BCNN_MODE_TRAIN) {
        iter->f_current = iter->f_train;
        bool valid = (iter->f_current != NULL);
        if (iter->has_extra_data) {
            iter->f_current_extra = iter->f_train_extra;
            valid = valid && (iter->f_current_extra != NULL);
        }
        BCNN_CHECK_AND_LOG(net->log_ctx, valid, BCNN_INVALID_DATA,
                           "A training dataset must be provided");
    } else {
        // We need to ensure that each time a prediction run is done, the same
        // data samples are being processed.
        rewind(iter->f_test);
        iter->f_current = iter->f_test;
        bool valid = (iter->f_current != NULL);
        if (has_extra) {
            iter->f_current_extra = iter->f_test_extra;
            valid = valid && (iter->f_current_extra != NULL);
        }
        BCNN_CHECK_AND_LOG(net->log_ctx, valid, BCNN_INVALID_DATA,
                           "A testing dataset must be provided");
    }

    return BCNN_SUCCESS;
}