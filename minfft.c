// A minimalistic FFT library.
// Written by Alexander Mukhin.
// Public domain.

#include "minfft.h"
#include <stdlib.h>
#include <math.h>
#include <tgmath.h>

const minfft_real pi=3.141592653589793238462643383279502884L;
const minfft_real sqrt2=1.414213562373095048801688724209698079L;

// *** meta-functions ***

// a pointer to a strided 1d complex transform routine
typedef
void (*strided_complex_1d_xform)
(minfft_cmpl*,minfft_cmpl*,int,const minfft_aux*);

// make a strided any-dimensional complex transform
// by repeated application of its strided one-dimensional routine
inline static void
make_complex_transform (
	minfft_cmpl *x, // source data
	minfft_cmpl *y, // destination buffer
	int sy, // stride on y
	const minfft_aux *a, // aux data
	strided_complex_1d_xform s_1d // strided 1d xform routine
) {
	if (a->sub2==NULL)
		(*s_1d)(x,y,sy,a);
	else {
		int N1=a->sub1->N,N2=a->sub2->N; // transform lengths
		int n; // counter
		minfft_cmpl *t=a->t; // temporary buffer
		for (n=0; n<N2; ++n)
			make_complex_transform(x+n*N1,t+n,N2,a->sub1,s_1d);
		for (n=0; n<N1; ++n)
			(*s_1d)(t+n*N2,y+sy*n,sy*N1,a->sub2);
	}
}

// a pointer to a strided 1d real transform routine
typedef
void (*strided_real_1d_xform)
(minfft_real*,minfft_real*,int,const minfft_aux*);

// make a strided any-dimensional real transform
// by repeated application of its strided one-dimensional routine
inline static void
make_real_transform (
	minfft_real *x, // source data
	minfft_real *y, // destination buffer
	int sy, // stride on y
	const minfft_aux *a, // aux data
	strided_real_1d_xform s_1d // strided 1d xform routine
) {
	if (a->sub2==NULL)
		(*s_1d)(x,y,sy,a);
	else {
		int N1=a->sub1->N,N2=a->sub2->N; // transform lengths
		int n; // counter
		minfft_real *t=a->t; // temporary buffer
		for (n=0; n<N2; ++n)
			make_real_transform(x+n*N1,t+n,N2,a->sub1,s_1d);
		for (n=0; n<N1; ++n)
			(*s_1d)(t+n*N2,y+sy*n,sy*N1,a->sub2);
	}
}

// *** complex transforms ***

// recursive strided one-dimensional DFT
inline static void
rs_dft_1d (
	int N, // transform length
	minfft_cmpl *x, // source data
	minfft_cmpl *t, // temporary buffer
	minfft_cmpl *y, // destination buffer
	int sy, // stride on y
	const minfft_cmpl *e // exponent vector
) {
	int n; // counter
	minfft_cmpl t0,t1,t2,t3; // temporary values
	// split-radix DIF
	if (N==1) {
		// trivial terminal case
		y[0] = x[0];
		return;
	}
	if (N==2) {
		// terminal case
		t0 = x[0]+x[1];
	  	t1 = x[0]-x[1];
		y[0] = t0;
	  	y[sy] = t1;
		return;
	}
	if (N==4) {
		// terminal case
		t0 = x[0]+x[2];
		t1 = x[1]+x[3];
		t2 = x[0]-x[2];
		t3 = I*(x[1]-x[3]);
		y[0] = t0+t1;
		y[sy] = t2-t3;
		y[2*sy] = t0-t1;
		y[3*sy] = t2+t3;
		return;
	}
	if (N==8) {
		// terminal case
		minfft_cmpl t00,t01,t02,t03;
		minfft_cmpl t10,t11,t12,t13;
		const minfft_cmpl E1=0.707106781186547524400844362104849039L*(1-I);
		const minfft_cmpl E3=0.707106781186547524400844362104849039L*(-1-I);
		t0 = x[0]+x[4];
		t1 = x[2]+x[6];
		t2 = x[0]-x[4];
		t3 = I*(x[2]-x[6]);
		t00 = t0+t1;
		t01 = t2-t3;
		t02 = t0-t1;
		t03 = t2+t3;
		t0 = x[1]+x[5];
		t1 = x[3]+x[7];
		t2 = x[1]-x[5];
		t3 = I*(x[3]-x[7]);
		t10 = t0+t1;
		t11 = (t2-t3)*E1;
		t12 = (t0-t1)*(-I);
		t13 = (t2+t3)*E3;
		y[0] = t00+t10;
		y[sy] = t01+t11;
		y[2*sy] = t02+t12;
		y[3*sy] = t03+t13;
		y[4*sy] = t00-t10;
		y[5*sy] = t01-t11;
		y[6*sy] = t02-t12;
		y[7*sy] = t03-t13;
		return;
	}
	// recursion
	// prepare sub-transform inputs
	for (n=0; n<N/4; ++n) {
		t0 = x[n]+x[n+N/2];
		t1 = x[n+N/4]+x[n+3*N/4];
		t2 = x[n]-x[n+N/2];
		t3 = I*(x[n+N/4]-x[n+3*N/4]);
		t[n] = t0;
		t[n+N/4] = t1;
		t[n+N/2] = (t2-t3)*e[2*n];
		t[n+3*N/4] = (t2+t3)*e[2*n+1];
	}
	// call sub-transforms
	rs_dft_1d(N/2,t,t,y,2*sy,e+N/2);
	rs_dft_1d(N/4,t+N/2,t+N/2,y+sy,4*sy,e+3*N/4);
	rs_dft_1d(N/4,t+3*N/4,t+3*N/4,y+3*sy,4*sy,e+3*N/4);
}

// strided one-dimensional DFT
inline static void
s_dft_1d (minfft_cmpl *x, minfft_cmpl *y, int sy, const minfft_aux *a) {
	rs_dft_1d(a->N,x,a->t,y,sy,a->e);
}

// strided DFT of arbitrary dimension
inline static void
s_dft (minfft_cmpl *x, minfft_cmpl *y, int sy, const minfft_aux *a) {
	make_complex_transform(x,y,sy,a,s_dft_1d);
}

// user interface
void
minfft_dft (minfft_cmpl *x, minfft_cmpl *y, const minfft_aux *a) {
	s_dft(x,y,1,a);
}

// recursive strided one-dimensional inverse DFT
inline static void
rs_invdft_1d (
	int N, // transform length
	minfft_cmpl *x, // source data
	minfft_cmpl *t, // temporary buffer
	minfft_cmpl *y, // destination buffer
	int sy, // stride on y
	const minfft_cmpl *e // exponent vector
) {
	int n; // counter
	minfft_cmpl t0,t1,t2,t3; // temporary values
	// split-radix DIF
	if (N==1) {
		// trivial terminal case
		y[0] = x[0];
		return;
	}
	if (N==2) {
		// terminal case
		t0 = x[0]+x[1];
	  	t1 = x[0]-x[1];
		y[0] = t0;
	  	y[sy] = t1;
		return;
	}
	if (N==4) {
		// terminal case
		t0 = x[0]+x[2];
		t1 = x[1]+x[3];
		t2 = x[0]-x[2];
		t3 = I*(x[1]-x[3]);
		y[0] = t0+t1;
		y[sy] = t2+t3;
		y[2*sy] = t0-t1;
		y[3*sy] = t2-t3;
		return;
	}
	if (N==8) {
		// terminal case
		minfft_cmpl t00,t01,t02,t03;
		minfft_cmpl t10,t11,t12,t13;
		const minfft_cmpl E1=0.707106781186547524400844362104849039L*(1+I);
		const minfft_cmpl E3=0.707106781186547524400844362104849039L*(-1+I);
		t0 = x[0]+x[4];
		t1 = x[2]+x[6];
		t2 = x[0]-x[4];
		t3 = I*(x[2]-x[6]);
		t00 = t0+t1;
		t01 = t2+t3;
		t02 = t0-t1;
		t03 = t2-t3;
		t0 = x[1]+x[5];
		t1 = x[3]+x[7];
		t2 = x[1]-x[5];
		t3 = I*(x[3]-x[7]);
		t10 = t0+t1;
		t11 = (t2+t3)*E1;
		t12 = (t0-t1)*I;
		t13 = (t2-t3)*E3;
		y[0] = t00+t10;
		y[sy] = t01+t11;
		y[2*sy] = t02+t12;
		y[3*sy] = t03+t13;
		y[4*sy] = t00-t10;
		y[5*sy] = t01-t11;
		y[6*sy] = t02-t12;
		y[7*sy] = t03-t13;
		return;
	}
	// recursion
	// prepare sub-transform inputs
	for (n=0; n<N/4; ++n) {
		t0 = x[n]+x[n+N/2];
		t1 = x[n+N/4]+x[n+3*N/4];
		t2 = x[n]-x[n+N/2];
		t3 = I*(x[n+N/4]-x[n+3*N/4]);
		t[n] = t0;
		t[n+N/4] = t1;
		t[n+N/2] = (t2+t3)*conj(e[2*n]);
		t[n+3*N/4] = (t2-t3)*conj(e[2*n+1]);
	}
	// call sub-transforms
	rs_invdft_1d(N/2,t,t,y,2*sy,e+N/2);
	rs_invdft_1d(N/4,t+N/2,t+N/2,y+sy,4*sy,e+3*N/4);
	rs_invdft_1d(N/4,t+3*N/4,t+3*N/4,y+3*sy,4*sy,e+3*N/4);
}

// strided one-dimensional inverse DFT
inline static void
s_invdft_1d (minfft_cmpl *x, minfft_cmpl *y, int sy, const minfft_aux *a) {
	rs_invdft_1d(a->N,x,a->t,y,sy,a->e);
}

// strided inverse DFT of arbitrary dimension
inline static void
s_invdft (minfft_cmpl *x, minfft_cmpl *y, int sy, const minfft_aux *a) {
	make_complex_transform(x,y,sy,a,s_invdft_1d);
}

// user interface
void
minfft_invdft (minfft_cmpl *x, minfft_cmpl *y, const minfft_aux *a) {
	s_invdft(x,y,1,a);
}

// *** real transforms ***

// strided one-dimensional real DFT
inline static void
s_realdft_1d (minfft_real *x, minfft_cmpl *z, int sz, const minfft_aux *a) {
	int n; // counter
	minfft_cmpl u,v; // temporary values
	int N=a->N; // transform length
	minfft_cmpl *e=a->e; // exponent vector
	minfft_cmpl *w=(minfft_cmpl*)x; // alias
	minfft_cmpl *t=a->t; // temporary buffer
	if (N==1) {
		// trivial case
		z[0] = x[0];
		return;
	}
	if (N==2) {
		// trivial case
		minfft_real t0,t1; // temporary values
		t0 = x[0];
		t1 = x[1];
		z[0] = t0+t1;
		z[sz] = t0-t1;
		return;
	}
	// reduce to complex DFT of length N/2
	// do complex DFT
	s_dft_1d(w,t,1,a->sub1);
	// recover results
	u = t[0];
	z[0] = creal(u)+cimag(u);
	z[sz*N/2] = creal(u)-cimag(u);
	for (n=1; n<N/4; ++n) {
		u = (t[n]+conj(t[N/2-n]))/2;
		v = (t[n]-conj(t[N/2-n]))*e[n]/(2*I);
		z[sz*n] = u+v;
		z[sz*(N/2-n)] = conj(u-v);
	}
	z[sz*N/4] = conj(t[N/4]);
}

// real DFT of arbitrary dimension
void
minfft_realdft (minfft_real *x, minfft_cmpl *z, const minfft_aux *a) {
	if (a->sub2==NULL)
		s_realdft_1d(x,z,1,a);
	else {
		int N1=a->sub1->N,N2=a->sub2->N; // transform lengths
		int n; // counter
		minfft_cmpl *t=a->t; // temporary buffer
		for (n=0; n<N2; ++n)
			s_realdft_1d(x+n*N1,t+n,N2,a->sub1);
		for (n=0; n<N1/2+1; ++n)
			s_dft(t+n*N2,z+n,N1/2+1,a->sub2);
	}
}

// strided one-dimensional inverse real DFT
inline static void
invrealdft_1d (minfft_cmpl *z, minfft_real *y, const minfft_aux *a) {
	int n; // counter
	minfft_cmpl u,v; // temporary values
	int N=a->N; // transform length
	minfft_cmpl *e=a->e; // exponent vector
	minfft_cmpl *w=(minfft_cmpl*)y; // alias
	if (N==1) {
		// trivial case
		y[0] = creal(z[0]);
		return;
	}
	if (N==2) {
		// trivial case
		minfft_real t0,t1; // temporary values
		t0 = creal(z[0]);
		t1 = creal(z[1]);
		y[0] = t0+t1;
		y[1] = t0-t1;
		return;
	}
	// reduce to inverse complex DFT of length N/2
	// prepare complex DFT inputs
	z[0] = (z[0]+z[N/2])+I*(z[0]-z[N/2]);
	for (n=1; n<N/4; ++n) {
		u = z[n]+conj(z[N/2-n]);
		v = I*(z[n]-conj(z[N/2-n]))*conj(e[n]);
		z[n] = u+v;
		z[N/2-n] = conj(u-v);
	}
	z[N/4] = 2*conj(z[N/4]);
	// do inverse complex DFT
	s_invdft_1d(z,w,1,a->sub1);
}

// inverse real DFT of arbitrary dimension
void
minfft_invrealdft (minfft_cmpl *z, minfft_real *y, const minfft_aux *a) {
	if (a->sub2==NULL)
		invrealdft_1d(z,y,a);
	else {
		int N1=a->sub1->N,N2=a->sub2->N; // transform lengths
		int n; // counter
		minfft_cmpl *t=a->t; // temporary buffer
		int k;
		for (n=0; n<N2; ++n)
			for (k=0; k<N1/2+1; ++k)
				t[n+N2*k] = z[(N1/2+1)*n+k];
		for (n=0; n<N1/2+1; ++n)
			s_invdft(t+n*N2,z+n,N1/2+1,a->sub2);
		for (n=0; n<N2; ++n)
			invrealdft_1d(z+n*(N1/2+1),y+n*N1,a->sub1);
	}
}

// *** real symmetric transforms ***

// strided one-dimensional DCT-2
inline static void
s_dct2_1d (minfft_real *x, minfft_real *y, int sy, const minfft_aux *a) {
	int n; // counter
	int N=a->N; // transform length
	minfft_real *t=a->t; // temporary buffer
	minfft_cmpl *z=(minfft_cmpl*)t; // its alias
	minfft_cmpl *e=a->e; // exponent vector
	minfft_cmpl u; // temporary value
	if (N==1) {
		// trivial case
		y[0] = 2*x[0];
		return;
	}
	// reduce to real DFT of length N
	// prepare sub-transform inputs
	for (n=0; n<N/2; ++n) {
		t[n] = x[2*n];
		t[N/2+n] = x[N-1-2*n];
	}
        // do real DFT in-place
	s_realdft_1d(t,z,1,a->sub1);
	// recover results
	y[0] = 2*creal(z[0]);
	for (n=1; n<N/2; ++n) {
		u = z[n]*e[n];
		y[sy*n] = 2*creal(u);
		y[sy*(N-n)] = -2*cimag(u);
	}
	y[sy*N/2] = sqrt2*creal(z[N/2]);
}

// strided DCT-2 of arbitrary dimension
inline static void
s_dct2 (minfft_real *x, minfft_real *y, int sy, const minfft_aux *a) {
	make_real_transform(x,y,sy,a,s_dct2_1d);
}

// user interface
void
minfft_dct2 (minfft_real *x, minfft_real *y, const minfft_aux *a) {
	s_dct2(x,y,1,a);
}

// strided one-dimensional DST-2
inline static void
s_dst2_1d (minfft_real *x, minfft_real *y, int sy, const minfft_aux *a) {
	int n; // counter
	int N=a->N; // transform length
	minfft_real *t=a->t; // temporary buffer
	minfft_cmpl *z=(minfft_cmpl*)t; // its alias
	minfft_cmpl *e=a->e; // exponent vector
	minfft_cmpl u; // temporary value
	if (N==1) {
		// trivial case
		y[0] = 2*x[0];
		return;
	}
	// reduce to real DFT of length N
	// prepare sub-transform inputs
	for (n=0; n<N/2; ++n) {
		t[n] = x[2*n];
		t[N/2+n] = -x[N-1-2*n];
	}
        // do real DFT in-place
	s_realdft_1d(t,z,1,a->sub1);
	// recover results
	y[sy*(N-1)] = 2*creal(z[0]);
	for (n=1; n<N/2; ++n) {
		u = z[n]*e[n];
		y[sy*(n-1)] = -2*cimag(u);
		y[sy*(N-n-1)] = 2*creal(u);
	}
	y[sy*(N/2-1)] = sqrt2*creal(z[N/2]);
}

// strided DST-2 of arbitrary dimension
inline static void
s_dst2 (minfft_real *x, minfft_real *y, int sy, const minfft_aux *a) {
	make_real_transform(x,y,sy,a,s_dst2_1d);
}

// user interface
void
minfft_dst2 (minfft_real *x, minfft_real *y, const minfft_aux *a) {
	s_dst2(x,y,1,a);
}

// strided one-dimensional DCT-3
inline static void
s_dct3_1d (minfft_real *x, minfft_real *y, int sy, const minfft_aux *a) {
	int n; // counter
	int N=a->N; // transform length
	minfft_cmpl *z=a->t; // temporary buffer
	minfft_real *t=(minfft_real*)z; // its alias
	minfft_cmpl *e=a->e; // exponent vector
	if (N==1) {
		// trivial case
		y[0] = x[0];
		return;
	}
	// reduce to inverse real DFT of length N
	// prepare sub-transform inputs
	z[0] = x[0];
	for (n=1; n<N/2; ++n)
		z[n] = conj((x[n]+I*x[N-n])*e[n]);
	z[N/2] = sqrt2*x[N/2];
	// do inverse real DFT in-place
	invrealdft_1d(z,t,a->sub1);
	// recover results
	for (n=0; n<N/2; ++n) {
		y[sy*2*n] = t[n];
		y[sy*(N-1-2*n)] = t[N/2+n];
	}
}

// strided DCT-3 of arbitrary dimension
inline static void
s_dct3 (minfft_real *x, minfft_real *y, int sy, const minfft_aux *a) {
	make_real_transform(x,y,sy,a,s_dct3_1d);
}

// user interface
void
minfft_dct3 (minfft_real *x, minfft_real *y, const minfft_aux *a) {
	s_dct3(x,y,1,a);
}

// strided one-dimensional DST-3
inline static void
s_dst3_1d (minfft_real *x, minfft_real *y, int sy, const minfft_aux *a) {
	int n; // counter
	int N=a->N; // transform length
	minfft_cmpl *z=a->t; // temporary buffer
	minfft_real *t=(minfft_real*)z; // its alias
	minfft_cmpl *e=a->e; // exponent vector
	if (N==1) {
		// trivial case
		y[0] = x[0];
		return;
	}
	// reduce to inverse real DFT of length N
	// prepare sub-transform inputs
	z[0] = x[N-1];
	for (n=1; n<N/2; ++n)
		z[n] = conj((x[N-n-1]+I*x[n-1])*e[n]);
	z[N/2] = sqrt2*x[N/2-1];
	// do inverse real DFT in-place
	invrealdft_1d(z,t,a->sub1);
	// recover results
	for (n=0; n<N/2; ++n) {
		y[sy*2*n] = t[n];
		y[sy*(N-1-2*n)] = -t[N/2+n];
	}
}

// strided DST-3 of arbitrary dimension
inline static void
s_dst3 (minfft_real *x, minfft_real *y, int sy, const minfft_aux *a) {
	make_real_transform(x,y,sy,a,s_dst3_1d);
}

// user interface
void
minfft_dst3 (minfft_real *x, minfft_real *y, const minfft_aux *a) {
	s_dst3(x,y,1,a);
}

// strided one-dimensional DCT-4
inline static void
s_dct4_1d (minfft_real *x, minfft_real *y, int sy, const minfft_aux *a) {
	int n; // counter
	int N=a->N; // transform length
	minfft_cmpl *t=a->t; // temporary buffer
	minfft_cmpl *e=a->e; // exponent vector
	if (N==1) {
		// trivial case
		y[0] = sqrt2*x[0];
		return;
	}
	// reduce to complex DFT of length N/2
	// prepare sub-transform inputs
	for (n=0; n<N/2; ++n)
		t[n] = *e++*(x[2*n]+I*x[N-1-2*n]);
	// do complex DFT in-place
	s_dft_1d(t,t,1,a->sub1);
	// recover results
	for (n=0; n<N/2; ++n) {
		y[sy*2*n] = 2*creal(*e++*t[n]);
		y[sy*(2*n+1)] = 2*creal(*e++*conj(t[N/2-1-n]));
	}
}

// strided DCT-4 of arbitrary dimension
inline static void
s_dct4 (minfft_real *x, minfft_real *y, int sy, const minfft_aux *a) {
	make_real_transform(x,y,sy,a,s_dct4_1d);
}

// user interface
void
minfft_dct4 (minfft_real *x, minfft_real *y, const minfft_aux *a) {
	s_dct4(x,y,1,a);
}

// strided one-dimensional DST-4
inline static void
s_dst4_1d (minfft_real *x, minfft_real *y, int sy, const minfft_aux *a) {
	int n; // counter
	int N=a->N; // transform length
	minfft_cmpl *t=a->t; // temporary buffer
	minfft_cmpl *e=a->e; // exponent vector
	if (N==1) {
		// trivial case
		y[0] = sqrt2*x[0];
		return;
	}
	// reduce to complex DFT of length N/2
	// prepare sub-transform inputs
	for (n=0; n<N/2; ++n)
		t[n] = -*e++*(x[2*n]-I*x[N-1-2*n]);
	// do complex DFT in-place
	s_dft_1d(t,t,1,a->sub1);
	// recover results
	for (n=0; n<N/2; ++n) {
		y[sy*2*n] = 2*cimag(*e++*t[n]);
		y[sy*(2*n+1)] = 2*cimag(*e++*conj(t[N/2-1-n]));
	}
}

// strided DST-4 of arbitrary dimension
inline static void
s_dst4 (minfft_real *x, minfft_real *y, int sy, const minfft_aux *a) {
	make_real_transform(x,y,sy,a,s_dst4_1d);
}

// user interface
void
minfft_dst4 (minfft_real *x, minfft_real *y, const minfft_aux *a) {
	s_dst4(x,y,1,a);
}

// *** making of aux data ***

// make aux data for any transform of arbitrary dimension
// using its one-dimensional version
static minfft_aux*
make_aux (int d, int *Ns, int datasz, minfft_aux* (*aux_1d)(int N)) {
	minfft_aux *a;
	int p; // product of all transform lengths
	int i; // array index
	if (d==1)
		return (*aux_1d)(Ns[0]);
	else {
		p = 1;
		for (i=0; i<d; ++i)
			p *= Ns[i];
		a = malloc(sizeof(minfft_aux));
		a->N = p;
		a->t = malloc(p*datasz);
		a->e = NULL;
		a->sub1 = make_aux(d-1,Ns,datasz,aux_1d);
		a->sub2 = (*aux_1d)(Ns[d-1]);
		return a;
	}
}

// make aux data for one-dimensional forward or inverse complex DFT
minfft_aux*
minfft_mkaux_dft_1d (int N) {
	minfft_aux *a;
	int n;
	minfft_cmpl *e;
	a = malloc(sizeof(minfft_aux));
	a->N = N;
	if (N>=16) {
		a->t = malloc(N*sizeof(minfft_cmpl));
		a->e = malloc(N*sizeof(minfft_cmpl));
		e = a->e;
		while (N>=16) {
			for (n=0; n<N/4; ++n) {
				*e++ = exp(-2*pi*I*n/N);
				*e++ = exp(-2*pi*I*3*n/N);
			}
			N /= 2;
		}
	} else {
		a->t = NULL;
		a->e = NULL;
	}
	a->sub1 = a->sub2 = NULL;
	return a;
}

// make aux data for any-dimensional forward or inverse complex DFT
minfft_aux*
minfft_mkaux_dft (int d, int *Ns) {
	return make_aux(d,Ns,sizeof(minfft_cmpl),minfft_mkaux_dft_1d);
}

// convenience routines for two- and three-dimensional complex DFT
minfft_aux*
minfft_mkaux_dft_2d (int N1, int N2) {
	int Ns[2]={N1,N2};
	return minfft_mkaux_dft(2,Ns);
}
minfft_aux*
minfft_mkaux_dft_3d (int N1, int N2, int N3) {
	int Ns[3]={N1,N2,N3};
	return minfft_mkaux_dft(3,Ns);
}

// make aux data for one-dimensional forward or inverse real DFT
minfft_aux*
minfft_mkaux_realdft_1d (int N) {
	minfft_aux *a;
	int n;
	minfft_cmpl *e;
	a = malloc(sizeof(minfft_aux));
	a->N = N;
	if (N>=4) {
		a->t = malloc((N/2)*sizeof(minfft_cmpl));
		a->e = malloc((N/4)*sizeof(minfft_cmpl));
		e = a->e;
		for (n=0; n<N/4; ++n)
			*e++ = exp(-2*pi*I*n/N);
		a->sub1 = minfft_mkaux_dft_1d(N/2);
	} else {
		a->t = NULL;
		a->e = NULL;
		a->sub1 = NULL;
	}
	a->sub2 = NULL;
	return a;
}

// make aux data for any-dimensional real DFT
minfft_aux*
minfft_mkaux_realdft (int d, int *Ns) {
	minfft_aux *a;
	int p; // product of transform lengths
	int i; // array index
	if (d==1)
		return minfft_mkaux_realdft_1d(Ns[0]);
	else {
		p = 1;
		for (i=1; i<d; ++i)
			p *= Ns[i];
		a = malloc(sizeof(minfft_aux));
		a->N = Ns[0]*p;
		a->t = malloc((Ns[0]/2+1)*p*sizeof(minfft_cmpl));
		a->e = NULL;
		a->sub1 = minfft_mkaux_realdft_1d(Ns[0]);
		a->sub2 = minfft_mkaux_dft(d-1,Ns+1);
		return a;
	}
}

// convenience routines for two- and three-dimensional real DFT
minfft_aux*
minfft_mkaux_realdft_2d (int N1, int N2) {
	int Ns[2]={N1,N2};
	return minfft_mkaux_realdft(2,Ns);
}
minfft_aux*
minfft_mkaux_realdft_3d (int N1, int N2, int N3) {
	int Ns[3]={N1,N2,N3};
	return minfft_mkaux_realdft(3,Ns);
}

// make aux data for one-dimensional Type-2 or Type-3 transforms
minfft_aux*
minfft_mkaux_t2t3_1d (int N) {
	minfft_aux *a;
	int n;
	minfft_cmpl *e;
	a = malloc(sizeof(minfft_aux));
	a->N = N;
	if (N>=2) {
		a->t = malloc((N+2)*sizeof(minfft_real)); // for in-place real DFT
		a->e = malloc((N/2)*sizeof(minfft_cmpl));
		e = a->e;
		for (n=0; n<N/2; ++n)
			*e++ = exp(-2*pi*I*n/(4*N));
	} else {
		a->t = NULL;
		a->e = NULL;
	}
	a->sub1 = minfft_mkaux_realdft_1d(N);
	a->sub2 = NULL;
	return a;
}

// make aux data for any-dimensional Type-2 or Type-3 transforms
minfft_aux*
minfft_mkaux_t2t3 (int d, int *Ns) {
	return make_aux(d,Ns,sizeof(minfft_real),minfft_mkaux_t2t3_1d);
}

// convenience routines for two- and three-dimensional Type 2 or 3 transforms
minfft_aux*
minfft_mkaux_t2t3_2d (int N1, int N2) {
	int Ns[2]={N1,N2};
	return minfft_mkaux_t2t3(2,Ns);
}
minfft_aux*
minfft_mkaux_t2t3_3d (int N1, int N2, int N3) {
	int Ns[3]={N1,N2,N3};
	return minfft_mkaux_t2t3(3,Ns);
}

// make aux data for an one-dimensional Type-4 transform
minfft_aux*
minfft_mkaux_t4_1d (int N) {
	minfft_aux *a;
	int n;
	minfft_cmpl *e;
	a = malloc(sizeof(minfft_aux));
	a->N = N;
	if (N>=2) {
		a->t = malloc((N/2)*sizeof(minfft_cmpl));
		a->e = malloc((N/2+N)*sizeof(minfft_cmpl));
		e = a->e;
		for (n=0; n<N/2; ++n)
			*e++ = exp(-2*pi*I*n/(2*N));
		for (n=0; n<N; ++n)
			*e++ = exp(-2*pi*I*(2*n+1)/(8*N));
	} else {
		a->t = NULL;
		a->e = NULL;
	}
	a->sub1 = minfft_mkaux_dft_1d(N/2);
	a->sub2 = NULL;
	return a;
}

// make aux data for an any-dimensional Type-4 transform
minfft_aux*
minfft_mkaux_t4 (int d, int *Ns) {
	return make_aux(d,Ns,sizeof(minfft_real),minfft_mkaux_t4_1d);
}

// convenience routines for two- and three-dimensional Type 4 transforms
minfft_aux*
minfft_mkaux_t4_2d (int N1, int N2) {
	int Ns[2]={N1,N2};
	return minfft_mkaux_t4(2,Ns);
}
minfft_aux*
minfft_mkaux_t4_3d (int N1, int N2, int N3) {
	int Ns[3]={N1,N2,N3};
	return minfft_mkaux_t4(3,Ns);
}

// free aux chain
void
minfft_free_aux (minfft_aux *a) {
	if (a->t)
		free(a->t);
	if (a->e)
		free(a->e);
	if (a->sub1)
		minfft_free_aux(a->sub1);
	if (a->sub2)
		minfft_free_aux(a->sub2);
	free(a);
}