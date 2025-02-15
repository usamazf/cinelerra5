
/*
 * CINELERRA
 * Copyright (C) 1997-2011 Adam Williams <broadcast at earthling dot net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "clip.h"
#include "fourier.h"
#include "samples.h"
#include "transportque.inc"

#define HALF_WINDOW (window_size / 2)


FFT::FFT()
{
}

FFT::~FFT()
{
}

void FFT::bit_reverse(unsigned int samples,
		double *real_in, double *imag_in,
		double *real_out, double *imag_out)
{
	unsigned int i, j;
	unsigned int num_bits = samples_to_bits(samples);
	unsigned int samples1 = samples - 1;
	if( !real_out ) real_out = real_in;
	if( real_out == real_in ) {  // Do in-place bit-reversal ordering
		if( !imag_in ) {
			imag_out[0] = 0.0;
			for( i=1; i<samples1; ++i ) {
				imag_out[i] = 0.0;
				j = reverse_bits(i, num_bits);
				if( i >= j ) continue;
				double tr = real_out[j];
				real_out[j] = real_out[i];  real_out[i] = tr;
			}
			imag_out[samples1] = 0.0;
		}
		else {
			for( i=1; i<samples1; ++i ) {
				j = reverse_bits(i, num_bits);
				if( i >= j ) continue;
				double tr = real_out[j], ti = imag_out[j];
				real_out[j] = real_out[i];  real_out[i] = tr;
				imag_out[j] = imag_out[i];  imag_out[i] = ti;
			}
		}
	}
	else {	// Do simultaneous data copy and bit-reversal ordering
		if( !imag_in ) {
			real_out[0] = real_in[0];
			imag_out[0] = 0.0;
			for( i=0; i<samples1; ++i ) {
				j = reverse_bits(i, num_bits);
				if( i > j ) continue;
				real_out[j] = real_in[i];
				real_out[i] = real_in[j];
				imag_out[i] = imag_out[j] = 0.0;
			}
			real_out[samples1] = real_in[samples1];
			imag_out[samples1] = 0.0;
		}
		else {
			for( i=0; i<samples; ++i ) {
				j = reverse_bits(i, num_bits);
				if( i > j ) continue;
				real_out[i] = real_in[j];  imag_out[i] = imag_in[j];
				real_out[j] = real_in[i];  imag_out[j] = imag_in[i];
			}
		}
	}
}

int FFT::do_fft(int samples,	// must be a power of 2
		int inverse,	// 0 = forward FFT, 1 = inverse
		double *real_in, double *imag_in, // complex input
		double *real_out, double *imag_out) // complex output
{
	bit_reverse(samples, real_in, imag_in, real_out, imag_out);
	double angle_numerator = !inverse ? 2*M_PI : -2*M_PI ;
	double ar[3], ai[3], rj, ij, rk, ik;
// In the first two passes, all of the multiplys, and half the adds
//  are saved by unrolling and reducing the x*1,x*0,x+0 operations.
	if( samples >= 2 ) { // block 1, delta_angle = pi
		for( int i=0; i<samples; i+=2 ) {
			int j = i, k = i+1; // cos(0)=1, sin(0)=0
			rj = real_out[j];  ij = imag_out[j];
			rk = real_out[k];  ik = imag_out[k];
			real_out[j] += rk;	  imag_out[j] += ik;
			real_out[k] = rj - rk;  imag_out[k] = ij - ik;
		}
	}
	if( samples >= 4 ) { // block 2, delta_angle = pi/2
		if( !inverse ) {
			for( int i=0; i<samples; i+=4 ) {
				int j = i, k = j+2; // cos(0)=1,sin(0)=0
				rj = real_out[j];  ij = imag_out[j];
				rk = real_out[k];  ik = imag_out[k];
				real_out[j] += rk;	  imag_out[j] += ik;
				real_out[k] = rj - rk;  imag_out[k] = ij - ik;
				j = i+1;  k = j+2; // cos(-pi/2)=0, sin(-pi/2)=-1
				rj = real_out[j];  ij = imag_out[j];
				rk = real_out[k];  ik = imag_out[k];
				real_out[j] += ik;	  imag_out[j] -= rk;
				real_out[k] = rj - ik;  imag_out[k] = ij + rk;
			}
		}
		else {
			for( int i=0; i<samples; i+=4 ) {
				int j = i, k = j+2; // cos(0)=1,sin(0)=0
				rj = real_out[j];  ij = imag_out[j];
				rk = real_out[k];  ik = imag_out[k];
				real_out[j] += rk;	  imag_out[j] += ik;
				real_out[k] = rj - rk;  imag_out[k] = ij - ik;
				j = i+1;  k = j+2; // cos(pi/2)=0, sin(pi/2)=1
				rj = real_out[j];  ij = imag_out[j];
				rk = real_out[k];  ik = imag_out[k];
				real_out[j] -= ik;	  imag_out[j] += rk;
				real_out[k] = rj + ik;  imag_out[k] = ij - rk;
			}
		}
	}
// Do the rest of the FFT
	for( int bsz=4,bsz2=2*bsz; bsz2<=samples; bsz2<<=1 ) { // block 3,..
		double delta_angle = angle_numerator / bsz2;
		double sm2 = sin(2 * delta_angle);
		double sm1 = sin(delta_angle);
		double cm2 = cos(2 * delta_angle);
		double cm1 = cos(delta_angle);
		double w = 2 * cm1;
		for( int i=0; i<samples; i+=bsz2 ) {
			ar[2] = cm2;  ar[1] = cm1;
			ai[2] = sm2;  ai[1] = sm1;

			double *rjp = real_out+i, *rkp = rjp + bsz;
			double *ijp = imag_out+i, *ikp = ijp + bsz;
			for( int n=bsz; --n>=0; ) {
				ar[0] = w * ar[1] - ar[2];
				ar[2] = ar[1];  ar[1] = ar[0];
				ai[0] = w * ai[1] - ai[2];
				ai[2] = ai[1];  ai[1] = ai[0];

				double rj = *rjp, ij = *ijp;
				double rk = *rkp, ik = *ikp;
				double tr = ar[0] * rk - ai[0] * ik;
				double ti = ar[0] * ik + ai[0] * rk;
				*rjp++ += tr;	  *ijp++ += ti;
				*rkp++ = rj - tr;  *ikp++ = ij - ti;
			}
		}

		bsz = bsz2;
	}
// Normalize if inverse transform
	if( inverse ) {
		double scale = 1./samples;
		for( int i=0; i<samples; ++i ) {
			real_out[i] *= scale;
			imag_out[i] *= scale;
		}
	}

	return 0;
}

int FFT::update_progress(int current_position)
{
	return 0;
}

unsigned int FFT::samples_to_bits(unsigned int samples)
{
	if( !samples ) return ~0;
	int i = 0;  --samples;
	while( (samples>>i) != 0 ) ++i;
	return i;
}

#if 0
unsigned int FFT::reverse_bits(unsigned int index, unsigned int bits)
{
	unsigned int i, rev;

	for( i = rev = 0; i < bits; i++ ) {
		rev = (rev << 1) | (index & 1);
		index >>= 1;
	}

	return rev;
}

#else

uint8_t FFT::rev_bytes[256] = {
  0x00, 0x80, 0x40, 0xc0, 0x20, 0xa0, 0x60, 0xe0, 0x10, 0x90, 0x50, 0xd0, 0x30, 0xb0, 0x70, 0xf0,
  0x08, 0x88, 0x48, 0xc8, 0x28, 0xa8, 0x68, 0xe8, 0x18, 0x98, 0x58, 0xd8, 0x38, 0xb8, 0x78, 0xf8,
  0x04, 0x84, 0x44, 0xc4, 0x24, 0xa4, 0x64, 0xe4, 0x14, 0x94, 0x54, 0xd4, 0x34, 0xb4, 0x74, 0xf4,
  0x0c, 0x8c, 0x4c, 0xcc, 0x2c, 0xac, 0x6c, 0xec, 0x1c, 0x9c, 0x5c, 0xdc, 0x3c, 0xbc, 0x7c, 0xfc,
  0x02, 0x82, 0x42, 0xc2, 0x22, 0xa2, 0x62, 0xe2, 0x12, 0x92, 0x52, 0xd2, 0x32, 0xb2, 0x72, 0xf2,
  0x0a, 0x8a, 0x4a, 0xca, 0x2a, 0xaa, 0x6a, 0xea, 0x1a, 0x9a, 0x5a, 0xda, 0x3a, 0xba, 0x7a, 0xfa,
  0x06, 0x86, 0x46, 0xc6, 0x26, 0xa6, 0x66, 0xe6, 0x16, 0x96, 0x56, 0xd6, 0x36, 0xb6, 0x76, 0xf6,
  0x0e, 0x8e, 0x4e, 0xce, 0x2e, 0xae, 0x6e, 0xee, 0x1e, 0x9e, 0x5e, 0xde, 0x3e, 0xbe, 0x7e, 0xfe,

  0x01, 0x81, 0x41, 0xc1, 0x21, 0xa1, 0x61, 0xe1, 0x11, 0x91, 0x51, 0xd1, 0x31, 0xb1, 0x71, 0xf1,
  0x09, 0x89, 0x49, 0xc9, 0x29, 0xa9, 0x69, 0xe9, 0x19, 0x99, 0x59, 0xd9, 0x39, 0xb9, 0x79, 0xf9,
  0x05, 0x85, 0x45, 0xc5, 0x25, 0xa5, 0x65, 0xe5, 0x15, 0x95, 0x55, 0xd5, 0x35, 0xb5, 0x75, 0xf5,
  0x0d, 0x8d, 0x4d, 0xcd, 0x2d, 0xad, 0x6d, 0xed, 0x1d, 0x9d, 0x5d, 0xdd, 0x3d, 0xbd, 0x7d, 0xfd,
  0x03, 0x83, 0x43, 0xc3, 0x23, 0xa3, 0x63, 0xe3, 0x13, 0x93, 0x53, 0xd3, 0x33, 0xb3, 0x73, 0xf3,
  0x0b, 0x8b, 0x4b, 0xcb, 0x2b, 0xab, 0x6b, 0xeb, 0x1b, 0x9b, 0x5b, 0xdb, 0x3b, 0xbb, 0x7b, 0xfb,
  0x07, 0x87, 0x47, 0xc7, 0x27, 0xa7, 0x67, 0xe7, 0x17, 0x97, 0x57, 0xd7, 0x37, 0xb7, 0x77, 0xf7,
  0x0f, 0x8f, 0x4f, 0xcf, 0x2f, 0xaf, 0x6f, 0xef, 0x1f, 0x9f, 0x5f, 0xdf, 0x3f, 0xbf, 0x7f, 0xff,
};

unsigned int FFT::reverse_bits(unsigned int index, unsigned int bits)
{
  unsigned char b;
  union { unsigned int u; uint8_t b[sizeof(unsigned int)]; } data;
  data.u = index;
  if( bits <= 8 ) {
	index = rev_bytes[data.b[0]] >> (8-bits);
  }
  else if( bits <= 16 ) {
	b = data.b[0];  data.b[0] = rev_bytes[data.b[1]];  data.b[1] = rev_bytes[b];
	index = data.u >> (16-bits);
  }
  else {
	b = data.b[0];  data.b[0] = rev_bytes[data.b[3]];  data.b[3] = rev_bytes[b];
	b = data.b[1];  data.b[1] = rev_bytes[data.b[2]];  data.b[2] = rev_bytes[b];
	index = data.u >> (32-bits);
  }
  return index;
}

#endif

int FFT::symmetry(int size, double *freq_real, double *freq_imag)
{
	int h = size / 2;
	for( int i = h + 1; i < size; i++ ) {
		freq_real[i] = freq_real[size - i];
		freq_imag[i] = -freq_imag[size - i];
	}
	return 0;
}





CrossfadeFFT::CrossfadeFFT() : FFT()
{
	bands = 1;
	reset();
	window_size = 4096;
}

CrossfadeFFT::~CrossfadeFFT()
{
	delete_fft();
}

int CrossfadeFFT::reset()
{
	input_buffer = 0;
	output_buffer = 0;
	freq_real = 0;
	freq_imag = 0;
	freq_real2 = 0;
	freq_imag2 = 0;
	output_real = 0;
	output_imag = 0;
	first_window = 1;
// samples in input_buffer and output_buffer
	input_size = 0;
	output_size = 0;
	input_allocation = 0;
	output_allocation = 0;
	output_sample = 0;
	input_sample = 0;
	return 0;
}

int CrossfadeFFT::delete_fft()
{
	delete input_buffer;	  input_buffer = 0;
	if( output_buffer ) {
		for( int i=0; i<bands; ++i ) delete [] output_buffer[i];
		delete [] output_buffer;  output_buffer = 0;
	}
	delete [] freq_real;	  freq_real = 0;
	delete [] freq_imag;	  freq_imag = 0;
	delete [] freq_real2;	 freq_real2 = 0;
	delete [] freq_imag2;	 freq_imag2 = 0;
	delete [] output_real;	output_real = 0;
	delete [] output_imag;	output_imag = 0;
	reset();
	return 0;
}

int CrossfadeFFT::fix_window_size()
{
// fix the window size
// window size must be a power of 2
	int new_size = 16;
	while(new_size < window_size) new_size *= 2;
	window_size = MIN(131072, window_size);
	window_size = new_size;
	return 0;
}

int CrossfadeFFT::initialize(int window_size, int bands)
{
	this->window_size = window_size;
	this->bands = bands;
	first_window = 1;
	reconfigure();
	return 0;
}

long CrossfadeFFT::get_delay()
{
	return window_size + HALF_WINDOW;
}

int CrossfadeFFT::reconfigure()
{
	delete_fft();
	fix_window_size();
	return 0;
}
void CrossfadeFFT::allocate_output(int new_allocation)
{
// Allocate output buffer
	if( new_allocation > output_allocation ) {
		if( !output_buffer ) {
			output_buffer = new double*[bands];
			bzero(output_buffer, sizeof(double) * bands);
		}

		for( int i = 0; i < bands; i++ ) {
			double *new_output = new double[new_allocation];
			if( output_buffer[i] ) {
				memcpy(new_output, output_buffer[i],
					sizeof(double) * (output_size + HALF_WINDOW));
				delete [] output_buffer[i];
			}
			output_buffer[i] = new_output;
		}
		output_allocation = new_allocation;
	}
}

int CrossfadeFFT::process_buffer(int64_t output_sample, long size,
		Samples *output_ptr, int direction)
{
	Samples *output_temp[1];
	output_temp[0] = output_ptr;
	return process_buffer(output_sample, size, output_temp, direction);
}

// int CrossfadeFFT::get_read_offset()
// {
// }

int CrossfadeFFT::process_buffer(int64_t output_sample,
		long size, Samples **output_ptr, int direction)
{
	int result = 0;
	int step = (direction == PLAY_FORWARD) ? 1 : -1;

// User seeked so output buffer is invalid
	if( output_sample != this->output_sample ) {
		output_size = 0;
		input_size = 0;
		first_window = 1;
		this->output_sample = output_sample;
		input_sample = output_sample;
	}


// printf("CrossfadeFFT::process_buffer %d size=%ld input_size=%ld output_size=%ld window_size=%ld\n",
// __LINE__, size, input_size, output_size, window_size);

// must call read_samples once so the upstream plugins don't have to seek
// must be a multiple of 1/2 window
	int need_samples = (size - output_size) / HALF_WINDOW * HALF_WINDOW;
// round up a half window
	if( need_samples + output_size < size ) {
		need_samples += HALF_WINDOW;
	}
// half window tail
	need_samples += HALF_WINDOW;

// extend the buffer to need_samples
	if( !input_buffer || input_buffer->get_allocated() < need_samples ) {
		Samples *new_input_buffer = new Samples(need_samples);

		if( input_buffer ) {
			memcpy(new_input_buffer->get_data(),
				input_buffer->get_data(),
				input_size * sizeof(double));
			delete input_buffer;
		}

		input_buffer = new_input_buffer;
	}

	input_buffer->set_offset(input_size);
	if( need_samples > input_size ) {
		result = read_samples(input_sample, need_samples-input_size, input_buffer);
		input_sample += step * (need_samples - input_size);
	}
	input_buffer->set_offset(0);
	input_size = need_samples;


	if( !freq_real ) freq_real = new double[window_size];
	if( !freq_imag ) freq_imag = new double[window_size];
	if( !freq_real2 ) freq_real2 = new double[window_size];
	if( !freq_imag2 ) freq_imag2 = new double[window_size];
	if( !output_real ) output_real = new double[window_size];
	if( !output_imag ) output_imag = new double[window_size];

// Fill output buffer half a window at a time until size samples are available
	while( !result && output_size < size ) {
		do_fft(window_size, 0,  // forward
			input_buffer->get_data(), 0, // input, real only
			freq_real2, freq_imag2); // output
		allocate_output(output_size + window_size);
// process & overlay each band separately
		for( int band=0; band<bands; ++band ) {
// restore from the backup
			memcpy(freq_real, freq_real2, sizeof(double) * window_size);
			memcpy(freq_imag, freq_imag2, sizeof(double) * window_size);
			signal_process(band);
			do_fft(window_size, 1, 	// inverse
				freq_real, freq_imag, // input
				output_real, output_imag); // output
			post_process(band);

// Overlay processed window on the output buffers
			if( first_window ) {
// direct copy the 1st window
				memcpy(output_buffer[band] + output_size,
					output_real,
					sizeof(double) * window_size);
			}
			else {
// dissolve 1st half of later windows
				for( int i = 0, j = output_size; i < HALF_WINDOW; i++, j++ ) {
					double src_level = (double)i / HALF_WINDOW;
					double dst_level = (double)(HALF_WINDOW - i) / HALF_WINDOW;
					output_buffer[band][j] = output_buffer[band][j] * dst_level +
						output_real[i] * src_level;
				}

//output_buffer[output_size] = 100.0;
//output_buffer[output_size + HALF_WINDOW] = -100.0;
// copy 2nd half of window
				memcpy(output_buffer[band] + output_size + HALF_WINDOW,
					output_real + HALF_WINDOW,
					sizeof(double) * HALF_WINDOW);
			}
		}

		first_window = 0;
		output_size += HALF_WINDOW;

// Shift input buffer half a window forward
		memcpy(input_buffer->get_data(),
			input_buffer->get_data() + HALF_WINDOW,
			(input_size - HALF_WINDOW) * sizeof(double));
// 		for( int i = HALF_WINDOW, j = 0;
// 			i < input_size;
// 			i++, j++ )
// 		{
// 			input_buffer->get_data()[j] = input_buffer->get_data()[i];
// 		}

		input_size -= HALF_WINDOW;
	}



// Transfer output buffer if the user wants it
	if( output_ptr ) {
		for( int band = 0; band < bands; band++ ) {
			memcpy(output_ptr[band]->get_data(), output_buffer[band], sizeof(double) * size);
		}
	}

// shift output buffers forward
	for( int band = 0; band < bands; band++ ) {
		memcpy(output_buffer[band], output_buffer[band] + size,
			sizeof(double) * (output_size + HALF_WINDOW - size));
	}

	this->output_sample += step * size;
	this->output_size -= size;

	return 0;
}


int CrossfadeFFT::read_samples(int64_t output_sample,
		int samples, Samples *buffer)
{
	return 1;
}

int CrossfadeFFT::signal_process()
{
	return 0;
}



int CrossfadeFFT::post_process()
{
	return 0;
}


int CrossfadeFFT::signal_process(int band)
{
	signal_process();
	return 0;
}



int CrossfadeFFT::post_process(int band)
{
	post_process();
	return 0;
}


