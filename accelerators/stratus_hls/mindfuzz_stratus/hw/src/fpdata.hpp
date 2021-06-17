// Copyright (c) 2011-2019 Columbia University, System Level Design Group
// SPDX-License-Identifier: Apache-2.0

#ifndef __NMF_FPDATA_HPP__
#define __NMF_FPDATA_HPP__

#include <cstdio>
#include <cstdlib>
#include <cstddef>
#include <systemc.h>

// Data types - modified from fft version to allow for either fixed pt or native
// further modified to allow for cynw floating pt

// for HLS we are interested in cynw (synthable) data types,
// either fixed or floating
#ifdef HLS_FP

#if (USE_FX == 1)
// Stratus fixed point
#include "cynw_fixed.h"

const unsigned int WORD_SIZE = FX_WIDTH;
// this part only applies to fixed point data
const unsigned int FPDATA_WL = FX_WIDTH;
#if (FX_WIDTH==64)
const unsigned int FPDATA_IL = FX64_IL;
#elif (FX_WIDTH==32)
const unsigned int FPDATA_IL = FX32_IL;
#elif (FX_WIDTH==16)
const unsigned int FPDATA_IL = FX16_IL;
#endif // FX_WIDTH
const unsigned int FPDATA_PL = (FPDATA_WL - FPDATA_IL);

// define the fixed point data type
typedef cynw_fixed<FPDATA_WL, FPDATA_IL, SC_RND> FPDATA;

// define the generic "TYPE" as the newly typedefd FPDATA
#define TYPE FPDATA

// define the data type that will be used for PLMs
typedef sc_dt::sc_int<WORD_SIZE> FPDATA_WORD;

// helper functions
template<typename T, size_t N>
T bv2fp(sc_dt::sc_bv<N> data_in)
{
    T data_out;

    {
        HLS_CONSTRAIN_LATENCY(0, HLS_ACHIEVABLE, "bv2fp1");

        for (unsigned i = 0; i < N; i++)
        {
            HLS_UNROLL_LOOP(ON, "bv2fp-loop");

            data_out[i] = data_in[i].to_bool();
        }
    }

    return data_out;
}

template<typename T, size_t N>
void bv2fp(T &data_out, sc_dt::sc_bv<N> data_in)
{
    {
        HLS_CONSTRAIN_LATENCY(0, HLS_ACHIEVABLE, "bv2fp2");

        for (unsigned i = 0; i < N; i++)
        {
            HLS_UNROLL_LOOP(ON, "bv2fp-loop");

            data_out[i] = data_in[i].to_bool();
        }
    }
}

template<typename T, size_t N>
sc_dt::sc_bv<N> fp2bv(T data_in)
{
    sc_dt::sc_bv<N> data_out;

    {
        HLS_CONSTRAIN_LATENCY(0, HLS_ACHIEVABLE, "fp2bv1");

        for (unsigned i = 0; i < N; i++)
        {
            HLS_UNROLL_LOOP(ON, "fp2bv-loop");

            data_out[i] = (bool) data_in[i];
        }
    }

    return data_out;
}

template<typename T, size_t N>
void fp2bv(sc_dt::sc_bv<N> &data_out, T data_in)
{
    {
        HLS_CONSTRAIN_LATENCY(0, HLS_ACHIEVABLE, "fp2bv2");

        for (unsigned i = 0; i < N; i++)
        {
            HLS_UNROLL_LOOP(ON, "fp2bv-loop");

            data_out[i] = (bool) data_in[i];
        }
    }
}

// Helper functions

// T <---> sc_dt::sc_int<N>

template<typename T, size_t N>
T int2fp(sc_dt::sc_int<N> data_in)
{
    T data_out;

    {
        HLS_CONSTRAIN_LATENCY(0, HLS_ACHIEVABLE, "int2fp1");

        for (unsigned i = 0; i < N; i++)
        {
            HLS_UNROLL_LOOP(ON, "int2fp-loop");

            data_out[i] = data_in[i].to_bool();
        }
    }

    return data_out;
}

template<typename T, size_t N>
void int2fp(T &data_out, sc_dt::sc_int<N> data_in)
{
    {
        HLS_CONSTRAIN_LATENCY(0, HLS_ACHIEVABLE, "int2fp2");

        for (unsigned i = 0; i < N; i++)
        {
            HLS_UNROLL_LOOP(ON, "int2fp-loop");

            data_out[i] = data_in[i].to_bool();
        }
    }
}

template<typename T, size_t N>
sc_dt::sc_int<N> fp2int(T data_in)
{
    sc_dt::sc_int<N> data_out;

    {
        HLS_CONSTRAIN_LATENCY(0, HLS_ACHIEVABLE, "fp2int1");

        for (unsigned i = 0; i < N; i++)
        {
            HLS_UNROLL_LOOP(ON, "fp2int-loop");

            data_out[i] = (bool) data_in[i];
        }
    }

    return data_out;
}

template<typename T, size_t N>
void fp2int(sc_dt::sc_int<N> &data_out, T data_in)
{
    {
        HLS_CONSTRAIN_LATENCY(0, HLS_ACHIEVABLE, "fp2int2");

        for (unsigned i = 0; i < N; i++)
        {
            HLS_UNROLL_LOOP(ON, "fp2int-loop");

            data_out[i] = (bool) data_in[i];
        }
    }
}

#else // !USE_FX
// Stratus floating point
#include "cynw_cm_float.h"

const unsigned int WORD_SIZE = FL_WIDTH;
// this part only applies to fixed point data
const unsigned int FPDATA_WL = FL_WIDTH;
#if (FL_WIDTH==32)
const unsigned int FPDATA_ML = FL32_ML;
#elif (FL_WIDTH==16)
const unsigned int FPDATA_ML = FL16_ML;
#elif (FL_WIDTH==8)
const unsigned int FPDATA_ML = FL08_ML;
#endif // FL_WIDTH
const unsigned int FPDATA_EL = (FPDATA_WL - FPDATA_ML - 1);

// define the floating point type
typedef cynw_cm_float<FPDATA_EL, FPDATA_ML, CYNW_BEST_ACCURACY, CYNW_NEAREST, 1> FPDATA;

// define the generic "TYPE" as the newly typedefd FPDATA
#define TYPE FPDATA

// define the data type that will be used for PLMs
typedef sc_dt::sc_uint<WORD_SIZE> FPDATA_WORD;

// Helper functions

// T <---> sc_dt::sc_uint<N>

template<typename T, size_t N>
T int2fp(sc_dt::sc_uint<N> data_in)
{
    T data_out;

    {
        HLS_CONSTRAIN_LATENCY(0, HLS_ACHIEVABLE, "uint2fp1");

        data_out.raw_bits(data_in);
        return data_out;
    }
}

template<typename T, size_t N>
void int2fp(T &data_out, sc_dt::sc_uint<N> data_in)
{
    {
        HLS_CONSTRAIN_LATENCY(0, HLS_ACHIEVABLE, "uint2fp2");

        data_out.raw_bits(data_in);
    }
}

template<typename T, size_t N>
sc_dt::sc_uint<N> fp2int(T data_in)
{
    {
        HLS_CONSTRAIN_LATENCY(0, HLS_ACHIEVABLE, "fp2uint1");

        return sc_dt::sc_uint<N>(data_in.raw_bits());
     }
}

template<typename T, size_t N>
void fp2int(sc_dt::sc_uint<N> &data_out, T data_in)
{
    {
        HLS_CONSTRAIN_LATENCY(0, HLS_ACHIEVABLE, "fp2uint2");

        data_out = sc_dt::sc_uint<N>(data_in.raw_bits());
     }
}


// T <---> sc_dt::sc_bv<N>

template<typename T, size_t N>
T bv2fp(sc_dt::sc_bv<N> data_in)
{
    T data_out;

    {
        HLS_CONSTRAIN_LATENCY(0, HLS_ACHIEVABLE, "bv2fp1");

        data_out.raw_bits(data_in);
        return data_out;
    }
}

template<typename T, size_t N>
void bv2fp(T &data_out, sc_dt::sc_bv<N> data_in)
{
    {
        HLS_CONSTRAIN_LATENCY(0, HLS_ACHIEVABLE, "bv2fp2");

        data_out.raw_bits(data_in.to_uint64());
    }
}

template<typename T, size_t N>
sc_dt::sc_bv<N> fp2bv(T data_in)
{
    {
        HLS_CONSTRAIN_LATENCY(0, HLS_ACHIEVABLE, "fp2bv1");

        return sc_dt::sc_bv<N>(data_in.raw_bits());
     }
}

template<typename T, size_t N>
void fp2bv(sc_dt::sc_bv<N> &data_out, T data_in)
{
    {
        HLS_CONSTRAIN_LATENCY(0, HLS_ACHIEVABLE, "fp2bv2");

        data_out = sc_dt::sc_bv<N>(data_in.raw_bits());
     }
}
       
#endif // USE_FX - end of cynw floating point

#else // !HLS_FP

// use native float instead - note that this requires 32 bit words
const unsigned int WORD_SIZE = 32;
#define TYPE float

// define data type for use in PLMs
typedef sc_dt::sc_int<WORD_SIZE> FPDATA_WORD;

// helper functions for native types

// inspired by conv_layer from SOC class
template<typename T, size_t N>
T bv2fp(sc_dt::sc_bv<N> data_in)
{
    uint32_t data = data_in.to_uint();
    T *ptr = (T *) &data;
    T data_out = *ptr;

    return data_out;
}

template<typename T, size_t N>
void bv2fp(T &data_out, sc_dt::sc_bv<N> data_in)
{
    uint32_t data = data_in.to_uint();
    T *ptr = (T *) &data;
    data_out = *ptr;
}

// TODO might be something wrong here
template<typename T, size_t N>
T int2fp(sc_dt::sc_int<N> data_in)
{
    uint32_t data = data_in.to_uint();
    T *ptr = (T *) &data;
    T data_out = *ptr;

    return data_out;
}

template<typename T, size_t N>
void int2fp(T &data_out, sc_dt::sc_int<N> data_in)
{
    uint32_t data = data_in.to_uint();
    T *ptr = (T *) &data;
    data_out = *ptr;
}

template<typename T, size_t N>
sc_dt::sc_bv<N> fp2bv(T data_in)
{
    uint32_t *ptr = (uint32_t *) &data_in;
    sc_dt::sc_bv<N> data_out;
    data_out = sc_dt::sc_bv<N>(*ptr);
    return data_out;
}

template<typename T, size_t N>
void fp2bv(sc_dt::sc_bv<N> &data_out, T data_in)
{
    uint32_t *ptr = (uint32_t *) &data_in;
    data_out = sc_dt::sc_bv<N>(*ptr);
}

// TODO something might be wrong here
template<typename T, size_t N>
sc_dt::sc_int<N> fp2int(T data_in)
{
    uint32_t *ptr = (uint32_t *) &data_in;
    sc_dt::sc_int<N> data_out;
    data_out = sc_dt::sc_int<N>(*ptr);
    return data_out;
}

template<typename T, size_t N>
void fp2int(sc_dt::sc_int<N> &data_out, T data_in)
{
    uint32_t *ptr = (uint32_t *) &data_in;
    data_out = sc_dt::sc_int<N>(*ptr);
}
#endif // HLS_FP

#endif // __NMF_FPDATA_HPP__
