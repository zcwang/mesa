/*
 * Copyright (C) 2018 Alyssa Rosenzweig
 * Copyright (C) 2019 Collabora, Ltd.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "compiler.h"
#include "midgard_ops.h"

bool
midgard_opt_copy_prop(compiler_context *ctx, midgard_block *block)
{
        bool progress = false;

        mir_foreach_instr_in_block_safe(block, ins) {
                if (ins->type != TAG_ALU_4) continue;
                if (!OP_IS_MOVE(ins->alu.op)) continue;

                unsigned from = ins->src[1];
                unsigned to = ins->dest;

                /* We only work on pure SSA */

                if (to >= SSA_FIXED_MINIMUM) continue;
                if (from >= SSA_FIXED_MINIMUM) continue;
                if (to & IS_REG) continue;
                if (from & IS_REG) continue;

                /* Constant propagation is not handled here, either */
                if (ins->has_inline_constant) continue;
                if (ins->has_constants) continue;

                /* Modifier propagation is not handled here */
                if (mir_nontrivial_source2_mod_simple(ins)) continue;
                if (mir_nontrivial_outmod(ins)) continue;

                /* Shortened arguments (bias for textures, extra load/store
                 * arguments, etc.) do not get a swizzle, only a start
                 * component and even that is restricted. Fragment writeout
                 * doesn't even get that much */

                bool skip = false;

                mir_foreach_instr_global(ctx, q) {
                        bool is_tex = q->type == TAG_TEXTURE_4;
                        bool is_ldst = q->type == TAG_LOAD_STORE_4;
                        bool is_branch = q->compact_branch;

                        if (!(is_tex || is_ldst || is_branch)) continue;

                        /* For textures, we get one real swizzle. For stores,
                         * we also get one. For loads, we get none. */

                        unsigned start =
                                is_tex ? 1 :
                                OP_IS_STORE(q->load_store.op) ? 1 : 0;

                        mir_foreach_src(q, s) {
                                if ((s >= start) && q->src[s] == to) {
                                        skip = true;
                                        break;
                                }
                        }
                }

                if (skip)
                        continue;

                /* We're clear -- rewrite, composing the swizzle */
                mir_rewrite_index_src_swizzle(ctx, to, from, ins->swizzle[1]);
                mir_remove_instruction(ins);
                progress |= true;
        }

        return progress;
}
