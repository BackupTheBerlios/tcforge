
static int intra_max_level[2][64] = 
{
	{
		27, 10,  5,  4,  3,  3,  3,  3, 
		2,  2,  1,  1,  1,  1,  1,  0,
		0,  0,  0,  0,  0,  0,  0,  0,
		0,  0,  0,  0,  0,  0,  0,  0,
		0,  0,  0,  0,  0,  0,  0,  0,
		0,  0,  0,  0,  0,  0,  0,  0,
		0,  0,  0,  0,  0,  0,  0,  0,
		0,  0,  0,  0,  0,  0,  0,  0,
	},

	{
		8,  3,  2,  2,  2,  2,  2,  1, 
		1,  1,  1,  1,  1,  1,  1,  1,
		1,  1,  1,  1,  1,  0,  0,  0,
		0,  0,  0,  0,  0,  0,  0,  0,
		0,  0,  0,  0,  0,  0,  0,  0,
		0,  0,  0,  0,  0,  0,  0,  0,
		0,  0,  0,  0,  0,  0,  0,  0,
		0,  0,  0,  0,  0,  0,  0,  0
	}
};


static int inter_max_level[2][64] = 
{
	{
		12,  6,  4,  3,  3,  3,  3,  2, 
		2,  2,  2,  1,  1,  1,  1,  1,
		1,  1,  1,  1,  1,  1,  1,  1,
		1,  1,  1,  0,  0,  0,  0,  0,
		0,  0,  0,  0,  0,  0,  0,  0,
		0,  0,  0,  0,  0,  0,  0,  0,
		0,  0,  0,  0,  0,  0,  0,  0,
		0,  0,  0,  0,  0,  0,  0,  0
	},

	{
		3,  2,  1,  1,  1,  1,  1,  1, 
		1,  1,  1,  1,  1,  1,  1,  1,
		1,  1,  1,  1,  1,  1,  1,  1,
		1,  1,  1,  1,  1,  1,  1,  1,
		1,  1,  1,  1,  1,  1,  1,  1,
		1,  0,  0,  0,  0,  0,  0,  0,
		0,  0,  0,  0,  0,  0,  0,  0,
		0,  0,  0,  0,  0,  0,  0,  0
	}
};


static int intra_max_run0[28] = 
{ 
	999, 14,  9,  7,  3,  2,  1,   
	1,  1,  1,  1,  0,  0,  0, 
	0,  0,  0,  0,  0,  0,  0,
	0,  0,  0,  0,  0,  0,  0
};


static int intra_max_run1[9] = 
{
	999, 20,  6,  
	1,  0,  0,  
	0,  0,  0
};


static int inter_max_run0[13] = 
{
	999, 
	26, 10,  6,  2,  1,  1,   
    0,  0,  0,  0,  0,  0
};


static int inter_max_run1[4] = 
{
	999, 40,  1,  0
};



