/*
	Copyright (C) 2022 Patrick Herlihy - https://byte4byte.com
	Copyright (C) 2022 DeSmuME team

	This file is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 2 of the License, or
	(at your option) any later version.

	This file is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with the this software.  If not, see <http://www.gnu.org/licenses/>.
*/

#define CHUNK_SIZE 6048*4

struct s_bytes {
	struct s_bytes *next;
    u8 data[CHUNK_SIZE];
};

typedef struct jumpmap {
	int num_bytes; // bytes in function where jump is
    int cond;
    u32 *instruct_ptr;
} t_jumpmap;

typedef struct labelmap {
	int num_bytes; // bytes in function where label is
    std::vector<t_jumpmap> jumps;
} t_labelmap;

static std::map<int, t_labelmap> g_LABELS;
static int g_NUMJUMPS = 0;
static int g_TOTALBYTES = 0;
u32 *g_PTR = NULL;
t_bytes *g_currBlock = NULL;

std::map<JittedFunc, u_int> allFuncs;

static void* alloc_executable_memory(size_t size) {
#ifdef __APPLE__
	void* ptr = mmap (NULL, size,
                    PROT_READ | PROT_WRITE | PROT_EXEC,
                    MAP_PRIVATE | MAP_ANONYMOUS |  MAP_JIT,
                    -1, 0);
#else
  void* ptr = mmap(0, size,
                   PROT_READ | PROT_WRITE | PROT_EXEC,
                MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
#endif
  if (ptr == (void*)-1) {
    return NULL;
  }
  if (! ptr) {
    return NULL;
  }

  allFuncs.insert(std::pair<JittedFunc,int>((JittedFunc)ptr, size));

  return ptr;
}

static char *g_curr_jit_block = NULL;
static char *g_curr_jit_ptr = NULL;
static int g_curr_jit_size = 0;

static void *armjitarm_alloc_func(size_t size) {
    if (! g_curr_jit_block || (uintptr_t)((uintptr_t)g_curr_jit_ptr + (uintptr_t)size) > (uintptr_t)((uintptr_t)g_curr_jit_block + (uintptr_t)g_curr_jit_size)) {
        if (g_curr_jit_block) {
            printf("RESETTING JIT\n");
            arm_jit_reset(1);
            //return NULL;
        }

        int memsize = 4 * 10000000;
        g_curr_jit_block = (char *)alloc_executable_memory(memsize);
        if (! g_curr_jit_block) return NULL;
        g_curr_jit_ptr = g_curr_jit_block;
        g_curr_jit_size = memsize;
    }

    char *ptr = g_curr_jit_ptr;
    g_curr_jit_ptr += size;
    return ptr;
}

static  void freeFuncs() {
	std::map<JittedFunc, u_int>::iterator it;
	for (it=allFuncs.begin(); it!=allFuncs.end(); ++it) {
		munmap((void *)it->first, (uintptr_t)it->second);
	}
	allFuncs.clear();
	g_curr_jit_block = NULL;
}

static void allocNewBlock() {
    if (! g_currBlock) {
        fprintf(stderr, "allocNewBlock failed\n");
        exit(0);
    }
    g_currBlock->next = (t_bytes *)malloc(sizeof(t_bytes));
    if (! g_currBlock->next) {
        fprintf(stderr, "allocNewBlock failed\n");
        exit(0);
    }
    g_currBlock = g_currBlock->next;
    g_PTR = (u32*)&g_currBlock->data[0];
    g_currBlock->next = NULL;
}

static  void output_w32(t_bytes *bytes, u32 word) {
    *g_PTR++ = word;
    g_TOTALBYTES+=4;
    if ((uintptr_t)g_PTR >= (uintptr_t)(&g_currBlock->data[0] + ((uintptr_t)CHUNK_SIZE))) {
        allocNewBlock();
    }
}

static  void emit_label(t_bytes *bytes, u_int id) {
    g_LABELS[id].num_bytes = g_TOTALBYTES + 4;
}

static  void emit_branch_label(t_bytes *out, u_int id, int cond) {	
    u32 *instruct_adr = g_PTR;

    output_w32(out, 0x00000000);

    int num_bytes = g_TOTALBYTES;

    t_jumpmap jm = {
        num_bytes,
        cond,
        instruct_adr
    };

    g_LABELS[id].jumps.push_back(jm);
}

static t_bytes *g_last_bytes = NULL;

t_bytes *allocBytes() {
    t_bytes *ret;	
    if (g_last_bytes) ret = g_last_bytes;
	else ret = (t_bytes *)malloc(sizeof(t_bytes));
    if (! ret) {
        fprintf(stderr, "allocBytes failed\n");
        exit(0);
    }

	ret->next = NULL;

    g_currBlock = ret;
    g_PTR = (u32*)&g_currBlock->data[0];
    g_TOTALBYTES = 0;

    g_LABELS.clear();

	return ret;
}

void releaseBytes(t_bytes *bytes) {
    g_last_bytes = bytes;
    bytes = bytes->next;
	while (bytes) {
		t_bytes *next = bytes->next;
		free(bytes);
		bytes = next;
	}
}
	
JittedFunc createFunc(t_bytes *bytes) {
    for (auto const& lbl : g_LABELS) {
        int nj = lbl.second.jumps.size();
        for (int i = 0; i < nj; i++) {
            MOD_JUMP(lbl.second.jumps[i].instruct_ptr, lbl.second.jumps[i].cond, lbl.second.num_bytes, lbl.second.jumps[i].num_bytes)
        }
    }

    char *fn = (char *)armjitarm_alloc_func(g_TOTALBYTES);
    char *fn_ptr = fn;
    if (! fn) return NULL;

    int tot_bytes_acum = 0;

    t_bytes *curr_bytes = bytes;
    while (curr_bytes) {
        if (curr_bytes->next) {
            memcpy(fn_ptr, curr_bytes->data, CHUNK_SIZE);
            fn_ptr += CHUNK_SIZE;
            tot_bytes_acum += CHUNK_SIZE;
        }
        else {
            memcpy(fn_ptr, curr_bytes->data, g_TOTALBYTES-tot_bytes_acum);
            break;
        }
        curr_bytes = curr_bytes->next;
    }
	
	__builtin___clear_cache(fn, fn+g_TOTALBYTES);
	
	return (JittedFunc)fn;
}
