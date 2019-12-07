#include "conv.h"
#include <ap_int.h>
#include <stdio.h>
#ifdef __SYNTHESIS__
#define BLOCKTYPE ap_fixed<16, 4>
#define BtZTYPE ap_fixed<18, 7>
#define UTYPE ap_fixed<18, 11>
#define WTYPE ap_fixed<16, -3>
#define UVTYPE ap_fixed<34, 8>
#define OUTTYPE ap_fixed<32, 6>
const int UNROLL_FACTOR=2;
#else
#define BLOCKTYPE ap_fixed<16, 4>
#define BtZTYPE ap_fixed<18, 7>
#define UTYPE ap_fixed<18, 11>
#define WTYPE ap_fixed<16, -3>
#define UVTYPE ap_fixed<34, 8>
#define OUTTYPE ap_fixed<32, 6>
#endif
const int Bt[6][6] = {
	{4, 0, -5, 0, 1, 0},
	{0, -4, -4, 1, 1, 0},
	{0, 4, -4, -1, 1, 0},
	{0, -2, -1, 2, 1, 0},
	{0, 2, -1, -2, 1, 0},
	{0, 4, 0, -5, 0, 1}
};
const int At[4][6] = {
	{1, 1, 1, 1, 1, 0},
	{0, 1, -1, 2, -2, 0},
	{0, 1, 1, 4, 4, 0},
	{0, 1, -1, 8, -8, 1}
};
const unsigned int bCHout = 64;
const unsigned int bCHin = 8;
const unsigned int bR_in = 6;
const unsigned int bC_in = 6;
const unsigned int KMax = 6;
const unsigned int SMin = 1;

const unsigned int tCHout = 64;
const unsigned int tCHin = 8;
const unsigned int tRo = 8;
const unsigned int tCo = 8;
const unsigned int tR_out = 4 * tRo;
const unsigned int tC_out = 4 * tCo;
const unsigned int tR_in = 2 + tR_out;
const unsigned int tC_in = 2 + tC_out;

const unsigned int bR_out = 4;
const unsigned int bC_out = 4;
const unsigned int vbR_out = 4;
const unsigned int vbC_out = 4;
const unsigned int vbR_in = 4;
const unsigned int vbC_in = 4;
unsigned CHin, CHout, R_in, C_in;
const unsigned int K = 3;
const unsigned int Kg = 6;
const unsigned int Ki = 6;
const unsigned int Ko = 4;
const unsigned int S = 1;

void load_w(d_type *W, WTYPE W_1[tCHout][tCHin][KMax][KMax], unsigned CHout_batch, unsigned CHin_batch, unsigned offset)
{
loop_W:
	for (unsigned i = 0; i < tCHout && i + CHout_batch < CHout; i++)
	{
		for (unsigned j = 0; j < tCHin && j + CHin_batch < CHin; j++)
		{
			for (unsigned k = 0; k < Kg; k++)
			{
				for (unsigned l = 0; l < Kg; l++)
				{
#pragma HLS PIPELINE
					W_1[i][j][k][l] = W[offset + i * (CHin * Kg * Kg) + j * Kg * Kg + k * Kg + l];
				}
			}
		}
	}
}

void load_in(d_type *In, BLOCKTYPE In_1[tCHin][tR_in][tC_in], unsigned R_in_batch, unsigned C_in_batch, unsigned CHin_batch, unsigned offset)
{
loop_In:
	for (unsigned j = 0; j < tR_in && j + R_in_batch < R_in; j++)
	{
		for (unsigned k = 0; k < tC_in && k + C_in_batch < C_in; k++)
		{
			for (unsigned i = 0; i < tCHin && i + CHin_batch < CHin; i++)
			{
#pragma HLS PIPELINE
				In_1[i][j][k] = In[offset + i * (R_in * C_in) + j * C_in + k];
			}
		}
	}
}

inline void ZtoU(BLOCKTYPE Z[6][6], UTYPE U[6][6])
{
	#pragma HLS inline
	BtZTYPE BtZ[6][6];

	for(int i = 0; i < 6; i++)
	{
	#pragma HLS unroll
		for(int j = 0; j < 6; j++)
		{
		#pragma HLS unroll
			U[i][j] = 0;
			BtZ[i][j] = 0;
		}
	}

	for(int i = 0; i < 6; i++)
	{
		#pragma HLS unroll
		for(int j = 0; j < 6; j++)
		{
			#pragma HLS unroll
			for(int k = 0; k < 6; k++)
			{
				#pragma HLS unroll
				BtZ[i][j] += Bt[i][k] * Z[k][j];
			}
		}
	}

	for(int i = 0; i < 6; i++)
	{
		#pragma HLS unroll
		for(int j = 0; j < 6; j++)
		{
			#pragma HLS unroll
			for(int k = 0; k < 6; k++)
			{
				#pragma HLS unroll
				U[i][j] += BtZ[i][k] * Bt[j][k];
			}
		}
	}
}

inline void UpointV(UTYPE U[6][6], WTYPE V[6][6], UVTYPE UV[6][6])
{
	#pragma HLS inline
	for (int i = 0; i < 6; i++)
	{
		#pragma HLS unroll
		for (int j = 0; j < 6; j++)
		{
			#pragma HLS unroll
			UV[i][j] = U[i][j] * V[i][j];
		}
	}
}

inline void UVtoY(UVTYPE UV[6][6], OUTTYPE Y[4][4])
{
	#pragma HLS inline
	UVTYPE AtUV[4][6];
	for(int i = 0; i < 4; i++)
	{
		#pragma HLS unroll
		for(int j = 0; j < 6; j++)
		{
			#pragma HLS unroll
			AtUV[i][j] = 0;
		}
	}
	
	for(int i = 0; i < 4; i++)
	{
		#pragma HLS unroll
		for(int j = 0; j < 6; j++)
		{
			#pragma HLS unroll
			for(int k = 0; k < 6; k++)
			{
				#pragma HLS unroll
				AtUV[i][j] += At[i][k] * UV[k][j];
			}
		}
	}

	for(int i = 0; i < 4; i++)
	{
		#pragma HLS unroll
		for(int j = 0; j < 4; j++)
		{
			#pragma HLS unroll
			for(int k = 0; k < 6; k++)
			{
				#pragma HLS unroll
				Y[i][j] += AtUV[i][k] * At[j][k];
			}
		}
	}	
}

void conv_batch(BLOCKTYPE In_1[tCHin][tR_in][tC_in], OUTTYPE Out_1[tCHout][tR_out][tC_out], WTYPE W_1[tCHout][tCHin][KMax][KMax], ap_int<8> CHin_batch)
{
	if (CHin_batch)
	{
		// 第二层tile
		loop_tRo:
		for (int ro = 0; ro < tRo; ro++)
		{
			loop_tCo:
			for (int co = 0; co < tCo; co++)
			{
			loop_CHin:
				for (unsigned chi = 0; chi < bCHin && chi + (CHin_batch - bCHin) < CHin; chi++)
				{
					BLOCKTYPE Z[6][6];
					loop_loadZ:
					for (int i = 0; i < 6; i++)
					{
						#pragma HLS unroll
						for (int j = 0; j < 6; j++)
						{
							#pragma HLS unroll
							Z[i][j] = In_1[chi][ro * 4 + i][co * 4 + j];
						}
					}
					UTYPE U[6][6];
					ZtoU(Z, U);
				loop_CHout:
					for (unsigned cho = 0; cho < bCHout; cho++)
					{
						#pragma HLS pipeline
						#pragma HLS unroll factor=UNROLL_FACTOR
						UVTYPE UV[6][6];
						UpointV(U, W_1[cho][chi], UV);
						OUTTYPE Y[4][4];
						loop_loadY:
						for(int i = 0; i < 4; i++)
						{
							#pragma HLS unroll
							for(int j = 0; j < 4; j++)
							{
								#pragma HLS unroll
								Y[i][j] = Out_1[cho][ro * 4 + i][co * 4 + j];
							}
						}
						UVtoY(UV, Y);
						loop_saveY:
						for(int i = 0; i < 4; i++)
						{
							#pragma HLS unroll
							for(int j = 0; j < 4; j++)
							{
								#pragma HLS unroll
								Out_1[cho][ro * 4 + i][co * 4 + j] = Y[i][j];
							}
						}
					}
				}
			}
		}
	}
}
void cnn(d_type *In, d_type *Out, d_type *W, int *Parameter)
{
#pragma HLS ALLOCATION instances = fmul limit = 32 operation
#pragma HLS ALLOCATION instances = fadd limit = 32 operation

/*
	In  : Input feature map, CHin*R*C
	Out : Output feature map, CHout*Rout*Cout
	W : weights, CHout*CHin*6*6
	Parameter:  CHin|CHout|R|C|K|S
	*/
#pragma HLS INTERFACE s_axilite port = return
#pragma HLS INTERFACE m_axi depth = 256 port = In offset = slave //adjust the depth as you need
#pragma HLS INTERFACE m_axi depth = 256 port = Out offset = slave
#pragma HLS INTERFACE m_axi depth = 256 port = W offset = slave
#pragma HLS INTERFACE m_axi depth = 256 port = Parameter offset = slave

	// 当前block size :
	/*
	CHin : 32
	CHout : 64
	R_in : 128
	C_in : 128
	R_out : ?
	C_out : ?
	K : 3
	S : 1
	*/

	BLOCKTYPE In_1[tCHin][tR_in][tC_in];
	OUTTYPE Out_1[tCHout][tR_out][tC_out];
	BLOCKTYPE In_0[tCHin][tR_in][tC_in];
	WTYPE W_0[tCHout][tCHin][KMax][KMax];
	WTYPE W_1[tCHout][tCHin][KMax][KMax];
// #pragma HLS RESOURCE variable=Out_1 core=RAM_1P_LUTRAM
#pragma HLS ARRAY_PARTITION variable = In_1 cyclic factor = 4 dim = 2
#pragma HLS ARRAY_PARTITION variable = In_1 cyclic factor = 4 dim = 3
#pragma HLS ARRAY_PARTITION variable = In_0 cyclic factor = 4 dim = 2
#pragma HLS ARRAY_PARTITION variable = In_0 cyclic factor = 4 dim = 3
// #pragma HLS ARRAY_PARTITION variable = In_1 complete dim=2
// #pragma HLS ARRAY_PARTITION variable = In_1 complete dim=3
// #pragma HLS ARRAY_PARTITION variable = In_0 complete dim=2
// #pragma HLS ARRAY_PARTITION variable = In_0 complete dim=3

#pragma HLS ARRAY_PARTITION variable = Out_1 cyclic factor=UNROLL_FACTOR dim = 1
#pragma HLS ARRAY_PARTITION variable = Out_1 cyclic factor = 4 dim=2
#pragma HLS ARRAY_PARTITION variable = Out_1 cyclic factor = 4 dim=3
// #pragma HLS ARRAY_PARTITION variable = Out_1 complete dim=2
// #pragma HLS ARRAY_PARTITION variable = Out_1 complete dim=3
#pragma HLS ARRAY_PARTITION variable = W_1 cyclic factor=UNROLL_FACTOR dim = 1
#pragma HLS ARRAY_PARTITION variable = W_0 cyclic factor=UNROLL_FACTOR dim = 1
#pragma HLS ARRAY_PARTITION variable = W_1 complete dim = 3
#pragma HLS ARRAY_PARTITION variable = W_1 complete dim = 4
#pragma HLS ARRAY_PARTITION variable = W_0 complete dim = 3
#pragma HLS ARRAY_PARTITION variable = W_0 complete dim = 4
	// #pragma HLS ARRAY_PARTITION variable=W_1 complete

	/*
	CHin : Input channels
	CHout : output channels
	R_in : Input rows
	C_in : Input columns
	K : kernel size (Kr = Kc)
	S : Stride
	*/

	CHin = Parameter[0];
	CHout = Parameter[1];
	R_in = Parameter[2];
	C_in = Parameter[3];
	// K = Parameter[4];
	// S = Parameter[5];

	unsigned R_out = (((unsigned)(R_in - K))) + 1;
	unsigned C_out = (((unsigned)(C_in - K))) + 1;

	for (unsigned R_in_batch = 0, R_out_batch = 0; R_out_batch < R_out; (R_in_batch += tR_out), (R_out_batch += tR_out))
	{
#pragma HLS LOOP_TRIPCOUNT max = 1
		for (unsigned C_in_batch = 0, C_out_batch = 0; C_out_batch < C_out; (C_in_batch += tC_out), (C_out_batch += tC_out))
		{
#pragma HLS LOOP_TRIPCOUNT max = 1
			for (unsigned CHout_batch = 0; CHout_batch < CHout; CHout_batch += tCHout)
			{
#pragma HLS LOOP_TRIPCOUNT max = 1
			loop_Out:
				for (unsigned r2 = 0; r2 < tR_out && r2 + R_out_batch < R_out; r2++)
				{
#pragma HLS LOOP_TRIPCOUNT max = 32
					for (unsigned c2 = 0; c2 < tC_out && c2 + C_out_batch < C_out; c2++)
					{
#pragma HLS LOOP_TRIPCOUNT max = 30
						for (unsigned cho = 0; cho < tCHout && cho + CHout_batch < CHout; cho++)
						{
#pragma HLS PIPELINE
							// Out_1[r2][c2][cho] = 0;
							Out_1[cho][r2][c2] = Out[(cho + CHout_batch) * R_out * C_out + (r2 + R_out_batch) * C_out + (c2 + C_out_batch)];
						}
					}
				}
				bool ping_pong_flag = 1;
				for (unsigned CHin_batch = 0; CHin_batch < CHin + tCHin /* ping pong add 1 */; CHin_batch += tCHin)
				{
#pragma HLS LOOP_TRIPCOUNT max = 5
					printf("FUCKYOU! %u %u %u %u\n", (unsigned)CHin_batch, (unsigned)CHout_batch, (unsigned)R_in_batch, (unsigned)C_in_batch);
					// #pragma HLS LOOP_FLATTEN OFF

					unsigned w_offset = CHout_batch * (CHin * Kg * Kg) + CHin_batch * (Kg * Kg);
					unsigned in_offset = CHin_batch * (R_in * C_in) + R_in_batch * C_in + C_in_batch;

					if (ping_pong_flag)
					{
						load_w(W, W_1, CHout_batch, CHin_batch, w_offset);
						load_in(In, In_1, R_in_batch, C_in_batch, CHin_batch, in_offset);
						conv_batch(In_0, Out_1, W_0, CHin_batch);
					}
					else
					{
						load_w(W, W_0, CHout_batch, CHin_batch, w_offset);
						load_in(In, In_0, R_in_batch, C_in_batch, CHin_batch, in_offset);
						conv_batch(In_1, Out_1, W_1, CHin_batch);
					}

					ping_pong_flag = !ping_pong_flag;
				}
			loop_AddedOut:
				for (unsigned r2 = 0; r2 < tR_out && r2 + R_out_batch < R_out; r2++)
				{
#pragma HLS LOOP_TRIPCOUNT max = 32
					for (unsigned c2 = 0; c2 < tC_out && c2 + C_out_batch < C_out; c2++)
					{
#pragma HLS LOOP_TRIPCOUNT max = 30
						for (unsigned cho = 0; cho < tCHout && cho + CHout_batch < CHout; cho++)
						{
#pragma HLS PIPELINE
							Out[(cho + CHout_batch) * R_out * C_out + (r2 + R_out_batch) * C_out + (c2 + C_out_batch)] = Out_1[cho][r2][c2];
						}
					}
				}
			}
		}
	}
}
