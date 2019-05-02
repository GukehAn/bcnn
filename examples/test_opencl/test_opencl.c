#include <float.h>
#include <math.h>
#include <stdio.h>

#include <bcnn/bcnn.h>
#include <bh/bh_macros.h>
#include <bh/bh_timer.h>
#include <bip/bip.h>

#include "bcnn_ocl_utils.h"
#include "bcnn_utils.h"

void show_usage(int argc, char **argv) {
    fprintf(stderr,
            "Usage: ./%s <input> <runs> [num_filters] [w_in] [h_in] [c_in]\n",
            argv[0]);
}

static float frand_between(float min, float max) {
    if (min > max) {
        return 0.f;
    }
    return ((float)rand() / RAND_MAX * (max - min)) + min + 0.5;
}

int main(int argc, char **argv) {
    if (argc < 2) {
        show_usage(argc, argv);
        return -1;
    }
    /* Load test image */
    unsigned char *img = NULL;
    int w, h, c;
    int ret = bip_load_image(argv[1], &img, &w, &h, &c);
    if (ret != BIP_SUCCESS) {
        fprintf(stderr, "[ERROR] Failed to open image %s\n", argv[1]);
        return -1;
    }
    /* Create net */
    bcnn_net *net = NULL;
    bcnn_init_net(&net, BCNN_MODE_PREDICT);
    fprintf(stderr, "Created net\n");
    int w_in = 128, h_in = 128, c_in = 128;
    if (argc >= 7) {
        w_in = atoi(argv[4]) > 0 ? atoi(argv[4]) : w_in;
        h_in = atoi(argv[5]) > 0 ? atoi(argv[5]) : h_in;
        c_in = atoi(argv[6]) > 0 ? atoi(argv[6]) : c_in;
    }
    bcnn_set_input_shape(net, w_in, h_in, c_in, 1);
    fprintf(stderr, "Set input shape: %d %d %d\n", w_in, h_in, c_in);
    int num_filters = 64;
    if (argc >= 4) {
        num_filters = atoi(argv[3]) > 0 ? atoi(argv[3]) : 64;
    }
    bcnn_add_convolutional_layer(net, num_filters, 3, 1, 1, 1, 0,
                                 BCNN_FILLER_XAVIER, BCNN_ACT_NONE, 0, "input",
                                 "out");
    fprintf(stderr, "Added conv layer\n");
    /* Compile net */
    if (bcnn_compile_net(net) != BCNN_SUCCESS) {
        bcnn_end_net(&net);
        return -1;
    }
    fprintf(stderr, "Compiled net\n");

    /* Get a pointer to the input tensor */
    bcnn_tensor *input_tensor = bcnn_get_tensor_by_name(net, "input");

    /* Check if input image depth is consistent */
    if (c != input_tensor->c) {
        fprintf(stderr, "Input random fill\n");
        /* Random fill */
        int sz = input_tensor->w * input_tensor->h * input_tensor->c *
                 input_tensor->n;
        for (int i = 0; i < sz; i++) {
            input_tensor->data[i] = frand_between(-1.f, 1.f);
        }
#ifdef BCNN_USE_OPENCL
        int rc = bcnn_opencl_memcpy_host2dev(net, input_tensor->data_gpu,
                                             input_tensor->data, sz);
        if (rc != 0) {
            fprintf(stderr, "Error while copying host buffer to device\n");
            return -2;
        }
#endif
    } else {
        /* Resize image if needed */
        if (input_tensor->w != w || input_tensor->h != h) {
            uint8_t *img_rz = (uint8_t *)calloc(
                input_tensor->w * input_tensor->h * c, sizeof(uint8_t));
            bip_resize_bilinear(img, w, h, w * c, img_rz, input_tensor->w,
                                input_tensor->h, input_tensor->w * c, c);
            free(img);
            img = img_rz;
        }

        /* Fill the input tensor with the current image data */
        float mean = 127.5f;
        float scale = 1 / 127.5f;
        bcnn_fill_tensor_with_image(net, img, input_tensor->w, input_tensor->h,
                                    c, scale, 0, mean, mean, mean,
                                    /*tensor_index=*/0, /*batch_index=*/0);
    }
    /* Setup timer */
    bh_timer t = {0};
    double elapsed_min = DBL_MAX;
    double elapsed_max = -DBL_MAX;
    double elapsed_avg = 0;
    int num_runs = 1;
    if (argc >= 3) {
        num_runs = atoi(argv[2]) > 0 ? atoi(argv[2]) : 1;
    }
    /* Inference runs */
    for (int i = 0; i < num_runs; ++i) {
        bh_timer_start(&t);
        /* Run the forward pass */
        bcnn_forward(net);
        bh_timer_stop(&t);
        double elapsed = bh_timer_get_msec(&t);
        elapsed_avg += elapsed;
        elapsed_min = bh_min(elapsed_min, elapsed);
        elapsed_max = bh_max(elapsed_max, elapsed);
    }
    elapsed_avg /= num_runs;
    fprintf(stderr, "img %s : min= %lf msecs max= %lf msecs avg= %lf msecs\n",
            argv[1], elapsed_min, elapsed_max, elapsed_avg);
    /* Get the output tensor pointer */
    /* Note: output tensor is expected to be named 'out' */
    bcnn_tensor *out = bcnn_get_tensor_by_name(net, "out");
    if (out != NULL) {
        float max_p = -1.f;
        int max_class = -1;
        for (int i = 0; i < out->c; ++i) {
            if (out->data[i] > max_p) {
                max_p = out->data[i];
                max_class = i;
            }
        }
    }
    /* Cleanup */
    bcnn_end_net(&net);
    free(img);
    return 0;
}