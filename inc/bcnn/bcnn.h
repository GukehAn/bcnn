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

#ifndef BCNN_H
#define BCNN_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Compiler and platform-specific preprocessor macros
 */

#include <stdio.h>

#if defined(__GNUC__) || (defined(_MSC_VER) && (_MSC_VER >= 1600))
#include <stdint.h>
#else
#if (_MSC_VER < 1300)
typedef signed char int8_t;
typedef signed short int16_t;
typedef signed int int32_t;
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;
#else
typedef signed __int8 int8_t;
typedef signed __int16 int16_t;
typedef signed __int32 int32_t;
typedef unsigned __int8 uint8_t;
typedef unsigned __int16 uint16_t;
typedef unsigned __int32 uint32_t;
#endif
typedef signed __int64 int64_t;
typedef unsigned __int64 uint64_t;
#endif

/**
 * Forward declarations of BCNN API struct
 */

/** Main BCNN object */
typedef struct bcnn_net bcnn_net;

/** Node struct */
typedef struct bcnn_node bcnn_node;

/** Tensor struct */
typedef struct bcnn_tensor bcnn_tensor;

/** Data loader handle structure */
typedef struct bcnn_loader bcnn_loader;

/** Data augmenter handle structure */
typedef struct bcnn_data_augmenter bcnn_data_augmenter;

/** Optimizer method and learning parameter handle structure */
typedef struct bcnn_learner bcnn_learner;

/** Simple bounding box container */
typedef struct bcnn_box bcnn_box;

/** Structure for object detection a la yolo */
typedef struct bcnn_yolo_detection bcnn_yolo_detection;

/** Logging context handle */
typedef struct bcnn_log_context bcnn_log_context;

/**
 * BCNN Enum types
 */

/**
 * Error codes.
 */
typedef enum {
    BCNN_SUCCESS,
    BCNN_INVALID_PARAMETER,
    BCNN_INVALID_DATA,
    BCNN_FAILED_ALLOC,
    BCNN_INTERNAL_ERROR,
    BCNN_CUDA_FAILED_ALLOC,
    BCNN_UNKNOWN_ERROR
} bcnn_status;

/**
 * The available modes that allow to switch between a inference-only framework
 * to a full training capable framework.
 */
typedef enum {
    BCNN_MODE_PREDICT, /* Deployment mode: Inference only (no auto-diff, no
                          groundtruth) */
    BCNN_MODE_TRAIN,   /* Training mode: Back-propagation, parameters update,
                          evaluation against ground-truth */
    BCNN_MODE_VALID    /* Evaluation mode: Forward pass and evaluation against
                          groundtruth but *no* back-propagation and *no* parameters
                          update */
} bcnn_mode;

/**
 * Data loader format
 */
typedef enum {
    BCNN_LOAD_MNIST,
    BCNN_LOAD_CIFAR10,
    BCNN_LOAD_CLASSIFICATION_LIST,
    BCNN_LOAD_REGRESSION_LIST,
    BCNN_LOAD_DETECTION_LIST,
    BCNN_NUM_LOADERS
} bcnn_loader_type;

/**
 * Enum of learning rate decay policies.
 */
typedef enum {
    BCNN_LR_DECAY_CONSTANT,
    BCNN_LR_DECAY_STEP,
    BCNN_LR_DECAY_INV,
    BCNN_LR_DECAY_EXP,
    BCNN_LR_DECAY_POLY,
    BCNN_LR_DECAY_SIGMOID
} bcnn_lr_decay;

/**
 * Enum of available layers types.
 */
typedef enum {
    BCNN_LAYER_CONV2D,
    BCNN_LAYER_TRANSPOSE_CONV2D,
    BCNN_LAYER_DEPTHWISE_CONV2D, /* Depthwise convolution */
    BCNN_LAYER_ACTIVATION,
    BCNN_LAYER_FULL_CONNECTED,
    BCNN_LAYER_MAXPOOL,
    BCNN_LAYER_AVGPOOL,
    BCNN_LAYER_SOFTMAX,
    BCNN_LAYER_DROPOUT,
    BCNN_LAYER_BATCHNORM,
    BCNN_LAYER_LRN,
    BCNN_LAYER_CONCAT,
    BCNN_LAYER_ELTWISE,
    BCNN_LAYER_UPSAMPLE,
    BCNN_LAYER_YOLOV3,
    BCNN_LAYER_RESHAPE,
    BCNN_LAYER_COST
} bcnn_layer_type;

/**
 * Enum of available activations functions (non-linearities).
 */
typedef enum {
    BCNN_ACT_NONE,
    BCNN_ACT_TANH,
    BCNN_ACT_RELU,
    BCNN_ACT_RAMP,
    BCNN_ACT_SOFTPLUS,
    BCNN_ACT_LRELU, /* Leaky relu (alpha (negative slope) set to 0.01) */
    BCNN_ACT_ABS,
    BCNN_ACT_CLAMP,
    BCNN_ACT_PRELU, /* Parametric ReLU */
    BCNN_ACT_LOGISTIC
} bcnn_activation;

/**
 *  Enum of available loss functions.
 */
typedef enum { BCNN_LOSS_EUCLIDEAN, BCNN_LOSS_LIFTED_STRUCT } bcnn_loss;

/**
 *  Enum of available error metrics.
 */
typedef enum {
    BCNN_METRIC_ERROR_RATE, /* Error rate (classification only) */
    BCNN_METRIC_LOGLOSS,    /* Multi-class Logloss (classification only) */
    BCNN_METRIC_SSE,        /* Sum-squared error */
    BCNN_METRIC_MSE,        /* Mean-squared error */
    BCNN_METRIC_CRPS,       /* Continuous Ranked Probability Score */
    BCNN_METRIC_DICE /* Sørensen–Dice index: metric for image segmentation
                      */
} bcnn_loss_metric;

/**
 * Available padding types
 *
 * Note: Currently used for pooling operation only. Convolutional-like
 * operations take explicit padding as input parameters.
 */
typedef enum {
    BCNN_PADDING_SAME,
    BCNN_PADDING_VALID,
    BCNN_PADDING_CAFFE /* Caffe-like padding for compatibility purposes */
} bcnn_padding;

/**
 *  Enum of optimization methods.
 */
typedef enum { BCNN_OPTIM_SGD, BCNN_OPTIM_ADAM } bcnn_optimizer;

/**
 * Available log levels.
 */
typedef enum {
    BCNN_LOG_INFO = 0,
    BCNN_LOG_WARNING = 1,
    BCNN_LOG_ERROR = 2,
    BCNN_LOG_SILENT = 3
} bcnn_log_level;

/**
 * The different type of tensor initialization.
 * This is ususally used to randomly initialize the weights/bias of one layer
 */
typedef enum bcnn_filler_type {
    BCNN_FILLER_FIXED,  /* Fill with constant value. For internal use only */
    BCNN_FILLER_XAVIER, /* Xavier init */
    BCNN_FILLER_MSRA    /* MSRA init */
} bcnn_filler_type;

/* Max number of bounding boxes for detection */
#define BCNN_DETECTION_MAX_BOXES 50

struct bcnn_loader {
    int n_samples;
    int input_width;
    int input_height;
    int input_depth;
    bcnn_loader_type type;
    uint8_t *input_uchar;
    uint8_t *input_net;
    FILE *f_input;
    FILE *f_label;
};

/**
 *  Structure for online data augmentation parameters.
 */
struct bcnn_data_augmenter {
    int range_shift_x;    /* X-shift allowed range (chosen between
                             [-range_shift_x / 2; range_shift_x / 2]). */
    int range_shift_y;    /* Y-shift allowed range (chosen between
                             [-range_shift_y / 2; range_shift_y / 2]). */
    int random_fliph;     /* If !=0, randomly (with probability of 0.5) apply
                             horizontal flip to image. */
    float min_scale;      /* Minimum scale factor allowed. */
    float max_scale;      /* Maximum scale factor allowed. */
    float rotation_range; /* Rotation angle allowed range (chosen between
                             [-rotation_range / 2; rotation_range / 2]).
                             Expressed in degree. */
    int min_brightness;   /* Minimum brightness factor allowed (additive factor,
                             range [-255;255]). */
    int max_brightness;   /* Maximum brightness factor allowed (additive factor,
                             range [-255;255]). */
    float min_contrast;   /* Minimum contrast allowed (mult factor). */
    float max_contrast;   /* Maximum contrast allowed (mult factor). */
    int use_precomputed;  /* Flag set to 1 if the parameters to be applied are
                             those already set. */
    float scale;          /* Current scale factor. */
    int shift_x;          /* Current x-shift. */
    int shift_y;          /* Current y-shift. */
    float rotation;       /* Current rotation angle. */
    int brightness;       /* Current brightness factor. */
    float contrast;       /* Current contrast factor. */
    float max_distortion; /* Maximum distortion factor allowed. */
    float distortion;     /* Current distortion factor. */
    float distortion_kx;  /* Current distortion x kernel. */
    float distortion_ky;  /* Current distortion y kernel. */
    int apply_fliph;      /* Current flip flag. */
    float mean_r;
    float mean_g;
    float mean_b;
    int swap_to_bgr;
    int no_input_norm;    /* If set to 1, Input data range is *not* normalized
                             between [-1;1] */
    int max_random_spots; /* Add a random number between [0;max_random_spots]
                             of saturated blobs. */
};

/**
 *  Structure to handle learner method and parameters.
 */
struct bcnn_learner {
    int step;
    int seen;            /* Number of instances seen by the network */
    int max_batches;     /* Maximum number of batches for training */
    float momentum;      /* Momentum parameter */
    float decay;         /* Decay parameter */
    float learning_rate; /* Base learning rate */
    float gamma;
    float scale;
    float power;
    float beta1;              /* Parameter for Adam optimizer */
    float beta2;              /* Parameter for Adam optimizer */
    bcnn_optimizer optimizer; /* Optimization method */
    bcnn_lr_decay decay_type; /* Learning rate decay type */
};

/* Logging callback */
typedef void (*bcnn_log_callback)(const char *fmt, ...);

struct bcnn_log_context {
    bcnn_log_callback fct;
    bcnn_log_level lvl;
};

/**
 * Tensor structure.
 * Data layout is NCHW.
 */
struct bcnn_tensor {
    int n;       /* Batch size */
    int c;       /* Number of channels = depth */
    int h;       /* Spatial height */
    int w;       /* Spatial width */
    float *data; /* Pointer to data */
#ifndef BCNN_DEPLOY_ONLY
    float *grad_data; /* Pointer to gradient data */
#endif
#ifdef BCNN_USE_CUDA
    float *data_gpu; /* Pointer to data on gpu */
#ifndef BCNN_DEPLOY_ONLY
    float *grad_data_gpu; /* Pointer to gradient data on gpu */
#endif
#endif
    int has_grad; /* If has gradient data or not */
    char *name;   /* Tensor name */
};

/**
 * Node definition
 */
struct bcnn_node {
    int num_src;
    int num_dst;
    bcnn_layer_type type;
    size_t param_size;
    int *src; /* Array of input tensors indexes */
    int *dst; /* Array of output tensors indexes */
    void *param;
    void (*forward)(struct bcnn_net *net, struct bcnn_node *node);
    void (*backward)(struct bcnn_net *net, struct bcnn_node *node);
    void (*update)(struct bcnn_net *net, struct bcnn_node *node);
    void (*release_param)(struct bcnn_node *node);
};

/**
 * Net definition
 */
struct bcnn_net {
    int batch_size;
    int num_nodes;
    bcnn_mode mode;
    bcnn_node *nodes;
    int num_tensors;              /* Number of tensors hold in the network */
    bcnn_tensor *tensors;         /* Array of tensors hold in the network */
    bcnn_data_augmenter data_aug; /* Parameters for online data augmentation */
    bcnn_learner learner;         /* Learner/optimizer parameters */
    bcnn_log_context log_ctx;
    void *gemm_ctx;
#ifdef BCNN_USE_CUDA
    void *cuda_ctx;
#endif
};

struct bcnn_box {
    float x, y, w, h;
};

struct bcnn_yolo_detection {
    bcnn_box bbox;
    int classes;
    float *prob;
    float *mask;
    float objectness;
    int sort_class;
};

/**
 *  This function creates an bcnn_net instance and needs to be called
 * before any other BCNN functions applied to this bcnn_net instance. In order
 * to free the bcnn_net instance, the function bcnn_end_net needs to be called
 * before exiting the application.
 *
 * Returns 'BCNN_SUCCESS' if successful initialization or 'BCNN_FAILED_ALLOC'
 * otherwise
 */
bcnn_status bcnn_init_net(bcnn_net **net);

/**
 * This function frees any allocated ressources in the bcnn_net instance
 * and destroys the instance itself (net pointer is set to NULL after being
 * freed).
 */
void bcnn_end_net(bcnn_net **net);

/* Logging */
void bcnn_set_log_context(bcnn_net *net, bcnn_log_callback fct,
                          bcnn_log_level level);

/**
 * Tensor manipulation helpers
 */
int bcnn_tensor_size(const bcnn_tensor *t);

int bcnn_tensor_size3d(const bcnn_tensor *t);

int bcnn_tensor_size2d(const bcnn_tensor *t);

/**
 * Set the shape of the primary input tensor
 */
void bcnn_set_input_shape(bcnn_net *net, int input_width, int input_height,
                          int input_channels, int batch_size);

/**
 * Add extra input tensors to the network
 */
bcnn_status bcnn_add_input(bcnn_net *net, int w, int h, int c, char *name);

int bcnn_set_param(bcnn_net *net, const char *name, const char *val);

bcnn_status bcnn_compile_net(bcnn_net *net);

bcnn_status bcnn_loader_initialize(bcnn_loader *iter, bcnn_loader_type type,
                                   bcnn_net *net, const char *path_input,
                                   const char *path_label);
bcnn_status bcnn_loader_next(bcnn_net *net, bcnn_loader *iter);
void bcnn_loader_terminate(bcnn_loader *iter);

/* Load / Write model */
bcnn_status bcnn_load_model(bcnn_net *net, char *filename);
bcnn_status bcnn_write_model(bcnn_net *net, char *filename);
/* For compatibility with older versions */
bcnn_status bcnn_load_model_legacy(bcnn_net *net, char *filename);

/* Conv layer */
bcnn_status bcnn_add_convolutional_layer(bcnn_net *net, int n, int size,
                                         int stride, int pad, int num_groups,
                                         int batch_norm, bcnn_filler_type init,
                                         bcnn_activation activation,
                                         int quantize, const char *src_id,
                                         const char *dst_id);

/* Transposed convolution 2d layer */
bcnn_status bcnn_add_deconvolutional_layer(
    bcnn_net *net, int n, int size, int stride, int pad, bcnn_filler_type init,
    bcnn_activation activation, const char *src_id, const char *dst_id);

/* Depthwise convolution layer */
bcnn_status bcnn_add_depthwise_conv_layer(bcnn_net *net, int size, int stride,
                                          int pad, int batch_norm,
                                          bcnn_filler_type init,
                                          bcnn_activation activation,
                                          const char *src_id,
                                          const char *dst_id);

/* Batchnorm layer */
bcnn_status bcnn_add_batchnorm_layer(bcnn_net *net, const char *src_id,
                                     const char *dst_id);

/* Local Response normalization layer */
bcnn_status bcnn_add_lrn_layer(bcnn_net *net, int local_size, float alpha,
                               float beta, float k, const char *src_id,
                               const char *dst_id);

/* Fully-connected layer */
bcnn_status bcnn_add_fullc_layer(bcnn_net *net, int output_size,
                                 bcnn_filler_type init,
                                 bcnn_activation activation, int quantize,
                                 const char *src_id, const char *dst_id);

/* Activation layer */
bcnn_status bcnn_add_activation_layer(bcnn_net *net, bcnn_activation type,
                                      const char *id);

/* Softmax layer */
bcnn_status bcnn_add_softmax_layer(bcnn_net *net, const char *src_id,
                                   const char *dst_id);

/* Max-Pooling layer */
bcnn_status bcnn_add_maxpool_layer(bcnn_net *net, int size, int stride,
                                   bcnn_padding padding, const char *src_id,
                                   const char *dst_id);

/* Average pooling layer */
bcnn_status bcnn_add_avgpool_layer(bcnn_net *net, const char *src_id,
                                   const char *dst_id);

/* Concat layer */
bcnn_status bcnn_add_concat_layer(bcnn_net *net, const char *src_id1,
                                  const char *src_id2, const char *dst_id);

/* Elementwise addition layer */
bcnn_status bcnn_add_eltwise_layer(bcnn_net *net, bcnn_activation activation,
                                   const char *src_id1, const char *src_id2,
                                   const char *dst_id);

/* Dropout layer */
bcnn_status bcnn_add_dropout_layer(bcnn_net *net, float rate, const char *id);

/* Upsample layer */
bcnn_status bcnn_add_upsample_layer(bcnn_net *net, int size, const char *src_id,
                                    const char *dst_id);

/* Cost layer */
bcnn_status bcnn_add_cost_layer(bcnn_net *net, bcnn_loss loss,
                                bcnn_loss_metric loss_metric, float scale,
                                const char *src_id, const char *label_id,
                                const char *dst_id);
/* Yolo output layer */
bcnn_status bcnn_add_yolo_layer(bcnn_net *net, int num_boxes_per_cell,
                                int classes, int coords, int total, int *mask,
                                float *anchors, const char *src_id,
                                const char *dst_id);

/* Return the detection results of a Yolo-like model */
bcnn_yolo_detection *bcnn_yolo_get_detections(bcnn_net *net, int batch, int w,
                                              int h, int netw, int neth,
                                              float thresh, int relative,
                                              int *num_dets);

/**
 * Convert an image (represented as an array of unsigned char) to floating point
 * values. Also perform a mean substraction and rescale the values
 * according to the following formula:
 * output_val = (input_pixel - mean) * norm_coeff
 *
 * Note: If the image has less than 3 channels, only the first mean values are
 * considered (up to the number of channels)
 *
 * @param[in]   src             Pointer to input image.
 * @param[in]   w               Image width.
 * @param[in]   h               Image height.
 * @param[in]   c               Number of channels of input image.
 * @param[in]   norm_coeff      Multiplicative factor to rescale input values
 * @param[in]   swap_to_bgr     Swap Red and Blue channels (Default layout is
 *                              RGB).
 * @param[in]   mean_r          Value to be substracted to first channel pixels
 *                              (red).
 * @param[in]   mean_g          Value to be substracted to second channel pixels
 *                              (green).
 * @param[in]   mean_b          Value to be substracted to third channel pixels
 * `                            (blue).
 * @param[out]  dst             Pointer to output float values array.
 */
void bcnn_convert_img_to_float(unsigned char *src, int w, int h, int c,
                               float norm_coeff, int swap_to_bgr, float mean_r,
                               float mean_g, float mean_b, float *dst);

/**
 * Perform the model prediction on the provided input data and computes the
 * loss if cost layers are defined.
 */
void bcnn_forward(bcnn_net *net);

/**
 * Back-propagate the gradients of the loss w.r.t. the parameters of the model.
 */
void bcnn_backward(bcnn_net *net);

/**
 * Update the model parameters according to the learning configuration and the
 * calculated gradients.
 */
void bcnn_update(bcnn_net *net);

/**
 * Convenient wrapper to compute the different steps required to train one batch
 * of data.
 * This functions performs the following:
 * - Load the next data batch (and performs data augmentation if required)
 * - Compute the forward pass given the loaded data batch
 * - Compute the back-propagation of the gradients
 * - Update the model parameters
 * - Return the loss according to the error metric
 *
 * The common use-case for this function is to be called inside a training loop
 * See: examples/mnist/mnist_example.c for a real-case example.
 */
float bcnn_train_on_batch(bcnn_net *net, bcnn_loader *data_load);

/**
 * Wrapper function to compute the inference pass only on a data batch.
 * This functions performs the following:
 * - Load the next data batch (and performs data augmentation if required)
 * - Compute the forward pass given the loaded data batch
 *
 * Return the loss value and the output raw data values.
 */
float bcnn_predict_on_batch(bcnn_net *net, bcnn_loader *data_load,
                            float **pred);

#ifdef __cplusplus
}
#endif

#endif  // BCNN_H
