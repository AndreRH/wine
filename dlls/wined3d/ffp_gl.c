/*
 * GL fixed-function pipeline
 *
 * Copyright 2002 Lionel Ulmer
 * Copyright 2002-2005 Jason Edmeades
 * Copyright 2003-2004 Raphael Junqueira
 * Copyright 2004 Christian Costa
 * Copyright 2005 Oliver Stieber
 * Copyright 2006 Henri Verbeet
 * Copyright 2006-2008 Stefan Dösinger for CodeWeavers
 * Copyright 2009-2011 Henri Verbeet for CodeWeavers
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */

#include <stdio.h>

#include "wined3d_private.h"
#include "wined3d_gl.h"

WINE_DEFAULT_DEBUG_CHANNEL(d3d);

/* Context activation for state handler is done by the caller. */

static void state_undefined(struct wined3d_context *context, const struct wined3d_state *state, DWORD state_id)
{
    ERR("Undefined state %s (%#lx).\n", debug_d3dstate(state_id), state_id);
}

void state_nop(struct wined3d_context *context, const struct wined3d_state *state, DWORD state_id)
{
    TRACE("%s: nop in current pipe config.\n", debug_d3dstate(state_id));
}

static void fillmode(const struct wined3d_rasterizer_state *r, const struct wined3d_gl_info *gl_info)
{
    enum wined3d_fill_mode mode = r ? r->desc.fill_mode : WINED3D_FILL_SOLID;

    switch (mode)
    {
        case WINED3D_FILL_POINT:
            gl_info->gl_ops.gl.p_glPolygonMode(GL_FRONT_AND_BACK, GL_POINT);
            checkGLcall("glPolygonMode(GL_FRONT_AND_BACK, GL_POINT)");
            break;
        case WINED3D_FILL_WIREFRAME:
            gl_info->gl_ops.gl.p_glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
            checkGLcall("glPolygonMode(GL_FRONT_AND_BACK, GL_LINE)");
            break;
        case WINED3D_FILL_SOLID:
            gl_info->gl_ops.gl.p_glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
            checkGLcall("glPolygonMode(GL_FRONT_AND_BACK, GL_FILL)");
            break;
        default:
            FIXME("Unrecognized fill mode %#x.\n", mode);
    }
}

static void cullmode(const struct wined3d_rasterizer_state *r, const struct wined3d_gl_info *gl_info)
{
    enum wined3d_cull mode = r ? r->desc.cull_mode : WINED3D_CULL_BACK;

    /* glFrontFace() is set in context.c at context init and on an
     * offscreen / onscreen rendering switch. */
    switch (mode)
    {
        case WINED3D_CULL_NONE:
            gl_info->gl_ops.gl.p_glDisable(GL_CULL_FACE);
            checkGLcall("glDisable GL_CULL_FACE");
            break;
        case WINED3D_CULL_FRONT:
            gl_info->gl_ops.gl.p_glEnable(GL_CULL_FACE);
            checkGLcall("glEnable GL_CULL_FACE");
            gl_info->gl_ops.gl.p_glCullFace(GL_FRONT);
            checkGLcall("glCullFace(GL_FRONT)");
            break;
        case WINED3D_CULL_BACK:
            gl_info->gl_ops.gl.p_glEnable(GL_CULL_FACE);
            checkGLcall("glEnable GL_CULL_FACE");
            gl_info->gl_ops.gl.p_glCullFace(GL_BACK);
            checkGLcall("glCullFace(GL_BACK)");
            break;
        default:
            FIXME("Unrecognized cull mode %#x.\n", mode);
    }
}

void state_shademode(struct wined3d_context *context, const struct wined3d_state *state, DWORD state_id)
{
    const struct wined3d_gl_info *gl_info = wined3d_context_gl(context)->gl_info;

    switch (state->render_states[WINED3D_RS_SHADEMODE])
    {
        case WINED3D_SHADE_FLAT:
            gl_info->gl_ops.gl.p_glShadeModel(GL_FLAT);
            checkGLcall("glShadeModel(GL_FLAT)");
            break;
        case WINED3D_SHADE_GOURAUD:
        /* WINED3D_SHADE_PHONG in practice is the same as WINED3D_SHADE_GOURAUD
         * in D3D. */
        case WINED3D_SHADE_PHONG:
            gl_info->gl_ops.gl.p_glShadeModel(GL_SMOOTH);
            checkGLcall("glShadeModel(GL_SMOOTH)");
            break;
        default:
            FIXME("Unrecognized shade mode %#x.\n",
                    state->render_states[WINED3D_RS_SHADEMODE]);
    }
}

static void state_ditherenable(struct wined3d_context *context, const struct wined3d_state *state, DWORD state_id)
{
    const struct wined3d_gl_info *gl_info = wined3d_context_gl(context)->gl_info;

    if (state->render_states[WINED3D_RS_DITHERENABLE])
    {
        gl_info->gl_ops.gl.p_glEnable(GL_DITHER);
        checkGLcall("glEnable GL_DITHER");
    }
    else
    {
        gl_info->gl_ops.gl.p_glDisable(GL_DITHER);
        checkGLcall("glDisable GL_DITHER");
    }
}

GLenum wined3d_gl_compare_func(enum wined3d_cmp_func f)
{
    switch (f)
    {
        case WINED3D_CMP_NEVER:
            return GL_NEVER;
        case WINED3D_CMP_LESS:
            return GL_LESS;
        case WINED3D_CMP_EQUAL:
            return GL_EQUAL;
        case WINED3D_CMP_LESSEQUAL:
            return GL_LEQUAL;
        case WINED3D_CMP_GREATER:
            return GL_GREATER;
        case WINED3D_CMP_NOTEQUAL:
            return GL_NOTEQUAL;
        case WINED3D_CMP_GREATEREQUAL:
            return GL_GEQUAL;
        case WINED3D_CMP_ALWAYS:
            return GL_ALWAYS;
        default:
            if (!f)
                WARN("Unrecognized compare function %#x.\n", f);
            else
                FIXME("Unrecognized compare function %#x.\n", f);
            return GL_NONE;
    }
}

static GLenum gl_blend_op(const struct wined3d_gl_info *gl_info, enum wined3d_blend_op op)
{
    switch (op)
    {
        case WINED3D_BLEND_OP_ADD:
            return GL_FUNC_ADD;
        case WINED3D_BLEND_OP_SUBTRACT:
            return gl_info->supported[EXT_BLEND_SUBTRACT] ? GL_FUNC_SUBTRACT : GL_FUNC_ADD;
        case WINED3D_BLEND_OP_REVSUBTRACT:
            return gl_info->supported[EXT_BLEND_SUBTRACT] ? GL_FUNC_REVERSE_SUBTRACT : GL_FUNC_ADD;
        case WINED3D_BLEND_OP_MIN:
            return gl_info->supported[EXT_BLEND_MINMAX] ? GL_MIN : GL_FUNC_ADD;
        case WINED3D_BLEND_OP_MAX:
            return gl_info->supported[EXT_BLEND_MINMAX] ? GL_MAX : GL_FUNC_ADD;
        default:
            if (!op)
                WARN("Unhandled blend op %#x.\n", op);
            else
                FIXME("Unhandled blend op %#x.\n", op);
            return GL_FUNC_ADD;
    }
}

static void blendop(const struct wined3d_state *state, const struct wined3d_gl_info *gl_info)
{
    const struct wined3d_blend_state *b = state->blend_state;
    GLenum blend_equation_alpha = GL_FUNC_ADD_EXT;
    GLenum blend_equation = GL_FUNC_ADD_EXT;

    if (!gl_info->supported[WINED3D_GL_BLEND_EQUATION])
    {
        WARN("Unsupported in local OpenGL implementation: glBlendEquation.\n");
        return;
    }

    /* BLENDOPALPHA requires GL_EXT_blend_equation_separate, so make sure it is around */
    if (b->desc.rt[0].op_alpha && !gl_info->supported[EXT_BLEND_EQUATION_SEPARATE])
    {
        WARN("Unsupported in local OpenGL implementation: glBlendEquationSeparate.\n");
        return;
    }

    blend_equation = gl_blend_op(gl_info, b->desc.rt[0].op);
    blend_equation_alpha = gl_blend_op(gl_info, b->desc.rt[0].op_alpha);
    TRACE("blend_equation %#x, blend_equation_alpha %#x.\n", blend_equation, blend_equation_alpha);

    if (b->desc.rt[0].op != b->desc.rt[0].op_alpha)
    {
        GL_EXTCALL(glBlendEquationSeparate(blend_equation, blend_equation_alpha));
        checkGLcall("glBlendEquationSeparate");
    }
    else
    {
        GL_EXTCALL(glBlendEquation(blend_equation));
        checkGLcall("glBlendEquation");
    }
}

static GLenum gl_blend_factor(enum wined3d_blend factor, const struct wined3d_format *dst_format)
{
    switch (factor)
    {
        case WINED3D_BLEND_ZERO:
            return GL_ZERO;
        case WINED3D_BLEND_ONE:
            return GL_ONE;
        case WINED3D_BLEND_SRCCOLOR:
            return GL_SRC_COLOR;
        case WINED3D_BLEND_INVSRCCOLOR:
            return GL_ONE_MINUS_SRC_COLOR;
        case WINED3D_BLEND_SRCALPHA:
            return GL_SRC_ALPHA;
        case WINED3D_BLEND_INVSRCALPHA:
            return GL_ONE_MINUS_SRC_ALPHA;
        case WINED3D_BLEND_DESTCOLOR:
            return GL_DST_COLOR;
        case WINED3D_BLEND_INVDESTCOLOR:
            return GL_ONE_MINUS_DST_COLOR;
        /* To compensate for the lack of format switching with backbuffer
         * offscreen rendering, and with onscreen rendering, we modify the
         * alpha test parameters for (INV)DESTALPHA if the render target
         * doesn't support alpha blending. A nonexistent alpha channel
         * returns 1.0, so WINED3D_BLEND_DESTALPHA becomes GL_ONE, and
         * WINED3D_BLEND_INVDESTALPHA becomes GL_ZERO. */
        case WINED3D_BLEND_DESTALPHA:
            return dst_format->alpha_size ? GL_DST_ALPHA : GL_ONE;
        case WINED3D_BLEND_INVDESTALPHA:
            return dst_format->alpha_size ? GL_ONE_MINUS_DST_ALPHA : GL_ZERO;
        case WINED3D_BLEND_SRCALPHASAT:
            return GL_SRC_ALPHA_SATURATE;
        case WINED3D_BLEND_BLENDFACTOR:
            return GL_CONSTANT_COLOR_EXT;
        case WINED3D_BLEND_INVBLENDFACTOR:
            return GL_ONE_MINUS_CONSTANT_COLOR_EXT;
        case WINED3D_BLEND_SRC1COLOR:
            return GL_SRC1_COLOR;
        case WINED3D_BLEND_INVSRC1COLOR:
            return GL_ONE_MINUS_SRC1_COLOR;
        case WINED3D_BLEND_SRC1ALPHA:
            return GL_SRC1_ALPHA;
        case WINED3D_BLEND_INVSRC1ALPHA:
            return GL_ONE_MINUS_SRC1_ALPHA;
        default:
            if (!factor)
                WARN("Unhandled blend factor %#x.\n", factor);
            else
                FIXME("Unhandled blend factor %#x.\n", factor);
            return GL_NONE;
    }
}

static void gl_blend_from_d3d(GLenum *src_blend, GLenum *dst_blend,
        enum wined3d_blend d3d_src_blend, enum wined3d_blend d3d_dst_blend,
        const struct wined3d_format *rt_format)
{
    /* WINED3D_BLEND_BOTHSRCALPHA and WINED3D_BLEND_BOTHINVSRCALPHA are legacy
     * source blending values which are still valid up to d3d9. They should
     * not occur as dest blend values. */
    if (d3d_src_blend == WINED3D_BLEND_BOTHSRCALPHA)
    {
        *src_blend = GL_SRC_ALPHA;
        *dst_blend = GL_ONE_MINUS_SRC_ALPHA;
    }
    else if (d3d_src_blend == WINED3D_BLEND_BOTHINVSRCALPHA)
    {
        *src_blend = GL_ONE_MINUS_SRC_ALPHA;
        *dst_blend = GL_SRC_ALPHA;
    }
    else
    {
        *src_blend = gl_blend_factor(d3d_src_blend, rt_format);
        *dst_blend = gl_blend_factor(d3d_dst_blend, rt_format);
    }
}

static void state_blend_factor_w(struct wined3d_context *context, const struct wined3d_state *state, DWORD state_id)
{
    WARN("Unsupported in local OpenGL implementation: glBlendColor.\n");
}

static void state_blend_factor(struct wined3d_context *context, const struct wined3d_state *state, DWORD state_id)
{
    const struct wined3d_gl_info *gl_info = wined3d_context_gl(context)->gl_info;
    const struct wined3d_color *factor = &state->blend_factor;

    TRACE("Setting blend factor to %s.\n", debug_color(factor));

    GL_EXTCALL(glBlendColor(factor->r, factor->g, factor->b, factor->a));
    checkGLcall("glBlendColor");
}

static BOOL is_blend_enabled(struct wined3d_context *context, const struct wined3d_state *state, UINT index)
{
    const struct wined3d_blend_state *b = state->blend_state;

    if (!state->fb.render_targets[index])
        return FALSE;

    if (!b->desc.rt[index].enable)
        return FALSE;

    /* Disable blending in all cases even without pixel shaders.
     * With blending on we could face a big performance penalty.
     * The d3d9 visual test confirms the behavior. */
    if (!(state->fb.render_targets[index]->format_caps & WINED3D_FORMAT_CAP_POSTPIXELSHADER_BLENDING))
        return FALSE;

    return TRUE;
}

static void blend(struct wined3d_context *context, const struct wined3d_state *state, DWORD state_id)
{
    const struct wined3d_gl_info *gl_info = wined3d_context_gl(context)->gl_info;
    const struct wined3d_blend_state *b = state->blend_state;
    const struct wined3d_format *rt_format;
    GLenum src_blend, dst_blend;
    unsigned int mask;

    if (gl_info->supported[ARB_MULTISAMPLE])
    {
        if (b && b->desc.alpha_to_coverage)
            gl_info->gl_ops.gl.p_glEnable(GL_SAMPLE_ALPHA_TO_COVERAGE);
        else
            gl_info->gl_ops.gl.p_glDisable(GL_SAMPLE_ALPHA_TO_COVERAGE);
        checkGLcall("glEnable GL_SAMPLE_ALPHA_TO_COVERAGE");
    }

    if (b && b->desc.independent)
        WARN("Independent blend is not supported by this GL implementation.\n");

    mask = b ? b->desc.rt[0].writemask : 0xf;

    gl_info->gl_ops.gl.p_glColorMask(mask & WINED3DCOLORWRITEENABLE_RED ? GL_TRUE : GL_FALSE,
            mask & WINED3DCOLORWRITEENABLE_GREEN ? GL_TRUE : GL_FALSE,
            mask & WINED3DCOLORWRITEENABLE_BLUE ? GL_TRUE : GL_FALSE,
            mask & WINED3DCOLORWRITEENABLE_ALPHA ? GL_TRUE : GL_FALSE);
    checkGLcall("glColorMask");

    if (!b || !is_blend_enabled(context, state, 0))
    {
        gl_info->gl_ops.gl.p_glDisable(GL_BLEND);
        checkGLcall("glDisable GL_BLEND");
        return;
    }

    gl_info->gl_ops.gl.p_glEnable(GL_BLEND);
    checkGLcall("glEnable GL_BLEND");

    rt_format = state->fb.render_targets[0]->format;

    gl_blend_from_d3d(&src_blend, &dst_blend, b->desc.rt[0].src, b->desc.rt[0].dst, rt_format);

    blendop(state, gl_info);

    if (b->desc.rt[0].src != b->desc.rt[0].src_alpha || b->desc.rt[0].dst != b->desc.rt[0].dst_alpha)
    {
        GLenum src_blend_alpha, dst_blend_alpha;

        /* Separate alpha blending requires GL_EXT_blend_function_separate, so make sure it is around */
        if (!gl_info->supported[EXT_BLEND_FUNC_SEPARATE])
        {
            WARN("Unsupported in local OpenGL implementation: glBlendFuncSeparate.\n");
            return;
        }

        gl_blend_from_d3d(&src_blend_alpha, &dst_blend_alpha, b->desc.rt[0].src_alpha, b->desc.rt[0].dst_alpha, rt_format);

        GL_EXTCALL(glBlendFuncSeparate(src_blend, dst_blend, src_blend_alpha, dst_blend_alpha));
        checkGLcall("glBlendFuncSeparate");
    }
    else
    {
        TRACE("glBlendFunc src=%x, dst=%x.\n", src_blend, dst_blend);
        gl_info->gl_ops.gl.p_glBlendFunc(src_blend, dst_blend);
        checkGLcall("glBlendFunc");
    }

    /* Colorkey fixup for stage 0 alphaop depends on blend state, so it may need
     * updating. */
    if (state->render_states[WINED3D_RS_COLORKEYENABLE])
        context_apply_state(context, state, STATE_TEXTURESTAGE(0, WINED3D_TSS_ALPHA_OP));
}

static void set_color_mask(const struct wined3d_gl_info *gl_info, UINT index, DWORD mask)
{
    GL_EXTCALL(glColorMaski(index,
            mask & WINED3DCOLORWRITEENABLE_RED ? GL_TRUE : GL_FALSE,
            mask & WINED3DCOLORWRITEENABLE_GREEN ? GL_TRUE : GL_FALSE,
            mask & WINED3DCOLORWRITEENABLE_BLUE ? GL_TRUE : GL_FALSE,
            mask & WINED3DCOLORWRITEENABLE_ALPHA ? GL_TRUE : GL_FALSE));
    checkGLcall("glColorMaski");
}

static void blend_db2(struct wined3d_context *context, const struct wined3d_state *state, DWORD state_id)
{
    const struct wined3d_gl_info *gl_info = wined3d_context_gl(context)->gl_info;
    GLenum src_blend, dst_blend, src_blend_alpha, dst_blend_alpha;
    const struct wined3d_blend_state *b = state->blend_state;
    const struct wined3d_format *rt_format;
    BOOL dual_source = b && b->dual_source;
    unsigned int i;

    if (b && b->desc.alpha_to_coverage)
        gl_info->gl_ops.gl.p_glEnable(GL_SAMPLE_ALPHA_TO_COVERAGE);
    else
        gl_info->gl_ops.gl.p_glDisable(GL_SAMPLE_ALPHA_TO_COVERAGE);
    checkGLcall("glEnable GL_SAMPLE_ALPHA_TO_COVERAGE");

    if (context->last_was_dual_source_blend != dual_source)
    {
        /* Dual source blending changes the location of the output varyings. */
        context->shader_update_mask |= 1u << WINED3D_SHADER_TYPE_PIXEL;
        context->last_was_dual_source_blend = dual_source;
    }

    if (!b || !b->desc.independent)
    {
        blend(context, state, state_id);
        return;
    }

    rt_format = state->fb.render_targets[0]->format;
    gl_blend_from_d3d(&src_blend, &dst_blend, b->desc.rt[0].src, b->desc.rt[0].dst, rt_format);
    gl_blend_from_d3d(&src_blend_alpha, &dst_blend_alpha, b->desc.rt[0].src_alpha, b->desc.rt[0].dst_alpha, rt_format);

    GL_EXTCALL(glBlendFuncSeparate(src_blend, dst_blend, src_blend_alpha, dst_blend_alpha));
    checkGLcall("glBlendFuncSeparate");

    GL_EXTCALL(glBlendEquationSeparate(gl_blend_op(gl_info, b->desc.rt[0].op),
            gl_blend_op(gl_info, b->desc.rt[0].op_alpha)));
    checkGLcall("glBlendEquationSeparate");

    for (i = 0; i < WINED3D_MAX_RENDER_TARGETS; ++i)
    {
        set_color_mask(gl_info, i, b->desc.rt[i].writemask);

        if (!is_blend_enabled(context, state, i))
        {
            GL_EXTCALL(glDisablei(GL_BLEND, i));
            checkGLcall("glDisablei GL_BLEND");
            continue;
        }

        GL_EXTCALL(glEnablei(GL_BLEND, i));
        checkGLcall("glEnablei GL_BLEND");

        if (b->desc.rt[i].src != b->desc.rt[0].src
                || b->desc.rt[i].dst != b->desc.rt[0].dst
                || b->desc.rt[i].op != b->desc.rt[0].op
                || b->desc.rt[i].src_alpha != b->desc.rt[0].src_alpha
                || b->desc.rt[i].dst_alpha != b->desc.rt[0].dst_alpha
                || b->desc.rt[i].op_alpha != b->desc.rt[0].op_alpha)
            WARN("Independent blend equations and blend functions are not supported by this GL implementation.\n");
    }

    /* Colorkey fixup for stage 0 alphaop depends on blend state, so it may need
     * updating. */
    if (state->render_states[WINED3D_RS_COLORKEYENABLE])
        context_apply_state(context, state, STATE_TEXTURESTAGE(0, WINED3D_TSS_ALPHA_OP));
}

static void blend_dbb(struct wined3d_context *context, const struct wined3d_state *state, DWORD state_id)
{
    const struct wined3d_gl_info *gl_info = wined3d_context_gl(context)->gl_info;
    const struct wined3d_blend_state *b = state->blend_state;
    BOOL dual_source = b && b->dual_source;
    unsigned int i;

    if (b && b->desc.alpha_to_coverage)
        gl_info->gl_ops.gl.p_glEnable(GL_SAMPLE_ALPHA_TO_COVERAGE);
    else
        gl_info->gl_ops.gl.p_glDisable(GL_SAMPLE_ALPHA_TO_COVERAGE);
    checkGLcall("glEnable GL_SAMPLE_ALPHA_TO_COVERAGE");

    if (context->last_was_dual_source_blend != dual_source)
    {
        /* Dual source blending changes the location of the output varyings. */
        context->shader_update_mask |= 1u << WINED3D_SHADER_TYPE_PIXEL;
        context->last_was_dual_source_blend = dual_source;
    }

    if (!b || !b->desc.independent)
    {
        blend(context, state, state_id);
        return;
    }

    for (i = 0; i < WINED3D_MAX_RENDER_TARGETS; ++i)
    {
        GLenum src_blend, dst_blend, src_blend_alpha, dst_blend_alpha;
        const struct wined3d_format *rt_format;

        set_color_mask(gl_info, i, b->desc.rt[i].writemask);

        if (!is_blend_enabled(context, state, i))
        {
            GL_EXTCALL(glDisablei(GL_BLEND, i));
            checkGLcall("glDisablei GL_BLEND");
            continue;
        }

        GL_EXTCALL(glEnablei(GL_BLEND, i));
        checkGLcall("glEnablei GL_BLEND");

        rt_format = state->fb.render_targets[i]->format;
        gl_blend_from_d3d(&src_blend, &dst_blend, b->desc.rt[i].src, b->desc.rt[i].dst, rt_format);
        gl_blend_from_d3d(&src_blend_alpha, &dst_blend_alpha,
                b->desc.rt[i].src_alpha, b->desc.rt[i].dst_alpha, rt_format);

        GL_EXTCALL(glBlendFuncSeparatei(i, src_blend, dst_blend, src_blend_alpha, dst_blend_alpha));
        checkGLcall("glBlendFuncSeparatei");

        GL_EXTCALL(glBlendEquationSeparatei(i, gl_blend_op(gl_info, b->desc.rt[i].op),
                gl_blend_op(gl_info, b->desc.rt[i].op_alpha)));
        checkGLcall("glBlendEquationSeparatei");
    }

    /* Colorkey fixup for stage 0 alphaop depends on blend state, so it may need
     * updating. */
    if (state->render_states[WINED3D_RS_COLORKEYENABLE])
        context_apply_state(context, state, STATE_TEXTURESTAGE(0, WINED3D_TSS_ALPHA_OP));
}

void state_alpha_test(struct wined3d_context *context, const struct wined3d_state *state, DWORD state_id)
{
    const struct wined3d_texture *texture = wined3d_state_get_ffp_texture(state, 0);
    const struct wined3d_gl_info *gl_info = wined3d_context_gl(context)->gl_info;
    int glParm = 0;
    float ref;
    BOOL enable_ckey = FALSE;

    TRACE("context %p, state %p, state_id %#lx.\n", context, state, state_id);

    /* Find out if the texture on the first stage has a ckey set. The alpha
     * state func reads the texture settings, even though alpha and texture
     * are not grouped together. This is to avoid making a huge alpha +
     * texture + texture stage + ckey block due to the hardly used
     * WINED3D_RS_COLORKEYENABLE state(which is d3d <= 3 only). The texture
     * function will call alpha in case it finds some texture + colorkeyenable
     * combination which needs extra care. */
    if (texture && (texture->async.color_key_flags & WINED3D_CKEY_SRC_BLT))
        enable_ckey = TRUE;

    if (enable_ckey || context->last_was_ckey)
        context_apply_state(context, state, STATE_TEXTURESTAGE(0, WINED3D_TSS_ALPHA_OP));
    context->last_was_ckey = enable_ckey;

    if (state->render_states[WINED3D_RS_ALPHATESTENABLE]
            || (state->render_states[WINED3D_RS_COLORKEYENABLE] && enable_ckey))
    {
        gl_info->gl_ops.gl.p_glEnable(GL_ALPHA_TEST);
        checkGLcall("glEnable GL_ALPHA_TEST");
    }
    else
    {
        gl_info->gl_ops.gl.p_glDisable(GL_ALPHA_TEST);
        checkGLcall("glDisable GL_ALPHA_TEST");
        /* Alpha test is disabled, don't bother setting the params - it will happen on the next
         * enable call
         */
        return;
    }

    if (state->render_states[WINED3D_RS_COLORKEYENABLE] && enable_ckey)
    {
        glParm = GL_NOTEQUAL;
        ref = 0.0f;
    }
    else
    {
        ref = wined3d_alpha_ref(state);
        glParm = wined3d_gl_compare_func(state->render_states[WINED3D_RS_ALPHAFUNC]);
    }
    if (glParm)
    {
        gl_info->gl_ops.gl.p_glAlphaFunc(glParm, ref);
        checkGLcall("glAlphaFunc");
    }
}

void state_clipping(struct wined3d_context *context, const struct wined3d_state *state, DWORD state_id)
{
    struct wined3d_context_gl *context_gl = wined3d_context_gl(context);
    uint32_t enable_mask;

    if (use_vs(state) && !context->d3d_info->vs_clipping)
    {
        static BOOL warned;

        /* The OpenGL spec says that clipping planes are disabled when using
         * shaders. Direct3D planes aren't, so that is an issue. The MacOS ATI
         * driver keeps clipping planes activated with shaders in some
         * conditions I got sick of tracking down. The shader state handler
         * disables all clip planes because of that - don't do anything here
         * and keep them disabled. */
        if (state->render_states[WINED3D_RS_CLIPPLANEENABLE] && !warned++)
            FIXME("Clipping not supported with vertex shaders.\n");
        return;
    }

    /* glEnable(GL_CLIP_PLANEx) doesn't apply to (ARB backend) vertex shaders.
     * The enabled / disabled planes are hardcoded into the shader. Update the
     * shader to update the enabled clipplanes. In case of fixed function, we
     * need to update the clipping field from ffp_vertex_settings. */
    context->shader_update_mask |= 1u << WINED3D_SHADER_TYPE_VERTEX;

    /* If enabling / disabling all
     * TODO: Is this correct? Doesn't D3DRS_CLIPPING disable clipping on the viewport frustrum?
     */
    enable_mask = state->render_states[WINED3D_RS_CLIPPING] ?
            state->render_states[WINED3D_RS_CLIPPLANEENABLE] : 0;
    wined3d_context_gl_enable_clip_distances(context_gl, enable_mask);
}

static void state_texfactor(struct wined3d_context *context, const struct wined3d_state *state, DWORD state_id)
{
    struct wined3d_context_gl *context_gl = wined3d_context_gl(context);
    const struct wined3d_gl_info *gl_info = context_gl->gl_info;
    struct wined3d_color color;
    unsigned int i;

    /* Note the texture color applies to all textures whereas
     * GL_TEXTURE_ENV_COLOR applies to active only. */
    wined3d_color_from_d3dcolor(&color, state->render_states[WINED3D_RS_TEXTUREFACTOR]);

    /* And now the default texture color as well */
    for (i = 0; i < context->d3d_info->ffp_fragment_caps.max_blend_stages; ++i)
    {
        /* Note the WINED3D_RS value applies to all textures, but GL has one
         * per texture, so apply it now ready to be used! */
        wined3d_context_gl_active_texture(context_gl, gl_info, i);

        gl_info->gl_ops.gl.p_glTexEnvfv(GL_TEXTURE_ENV, GL_TEXTURE_ENV_COLOR, &color.r);
        checkGLcall("glTexEnvfv(GL_TEXTURE_ENV, GL_TEXTURE_ENV_COLOR, color);");
    }
}

static void renderstate_stencil_twosided(struct wined3d_context *context, GLint face,
        GLint func, GLint ref, GLuint mask, GLint stencilFail, GLint depthFail, GLint stencilPass)
{
    const struct wined3d_gl_info *gl_info = wined3d_context_gl(context)->gl_info;

    gl_info->gl_ops.gl.p_glEnable(GL_STENCIL_TEST_TWO_SIDE_EXT);
    checkGLcall("glEnable(GL_STENCIL_TEST_TWO_SIDE_EXT)");
    GL_EXTCALL(glActiveStencilFaceEXT(face));
    checkGLcall("glActiveStencilFaceEXT(...)");
    gl_info->gl_ops.gl.p_glStencilFunc(func, ref, mask);
    checkGLcall("glStencilFunc(...)");
    gl_info->gl_ops.gl.p_glStencilOp(stencilFail, depthFail, stencilPass);
    checkGLcall("glStencilOp(...)");
}

static GLenum gl_stencil_op(enum wined3d_stencil_op op)
{
    switch (op)
    {
        case WINED3D_STENCIL_OP_KEEP:
            return GL_KEEP;
        case WINED3D_STENCIL_OP_ZERO:
            return GL_ZERO;
        case WINED3D_STENCIL_OP_REPLACE:
            return GL_REPLACE;
        case WINED3D_STENCIL_OP_INCR_SAT:
            return GL_INCR;
        case WINED3D_STENCIL_OP_DECR_SAT:
            return GL_DECR;
        case WINED3D_STENCIL_OP_INVERT:
            return GL_INVERT;
        case WINED3D_STENCIL_OP_INCR:
            return GL_INCR_WRAP;
        case WINED3D_STENCIL_OP_DECR:
            return GL_DECR_WRAP;
        default:
            if (!op)
                WARN("Unrecognized stencil op %#x.\n", op);
            else
                FIXME("Unrecognized stencil op %#x.\n", op);
            return GL_KEEP;
    }
}

static void state_stencil(struct wined3d_context *context, const struct wined3d_state *state)
{
    const struct wined3d_gl_info *gl_info = wined3d_context_gl(context)->gl_info;
    const struct wined3d_depth_stencil_state *d = state->depth_stencil_state;
    GLint func;
    GLint func_back;
    GLint ref;
    GLuint mask;
    GLint stencilFail;
    GLint stencilFail_back;
    GLint stencilPass;
    GLint stencilPass_back;
    GLint depthFail;
    GLint depthFail_back;

    /* No stencil test without a stencil buffer. */
    if (!state->fb.depth_stencil || !d || !d->desc.stencil)
    {
        gl_info->gl_ops.gl.p_glDisable(GL_STENCIL_TEST);
        checkGLcall("glDisable GL_STENCIL_TEST");
        return;
    }

    if (!(func = wined3d_gl_compare_func(d->desc.front.func)))
        func = GL_ALWAYS;
    if (!(func_back = wined3d_gl_compare_func(d->desc.back.func)))
        func_back = GL_ALWAYS;
    mask = d->desc.stencil_read_mask;
    ref = state->stencil_ref & wined3d_mask_from_size(state->fb.depth_stencil->format->stencil_size);
    stencilFail = gl_stencil_op(d->desc.front.fail_op);
    depthFail = gl_stencil_op(d->desc.front.depth_fail_op);
    stencilPass = gl_stencil_op(d->desc.front.pass_op);
    stencilFail_back = gl_stencil_op(d->desc.back.fail_op);
    depthFail_back = gl_stencil_op(d->desc.back.depth_fail_op);
    stencilPass_back = gl_stencil_op(d->desc.back.pass_op);

    TRACE("(ref %x, mask %x, "
            "GL_FRONT: func: %x, fail %x, zfail %x, zpass %x "
            "GL_BACK: func: %x, fail %x, zfail %x, zpass %x)\n",
            ref, mask,
            func, stencilFail, depthFail, stencilPass,
            func_back, stencilFail_back, depthFail_back, stencilPass_back);

    if (memcmp(&d->desc.front, &d->desc.back, sizeof(d->desc.front)))
    {
        gl_info->gl_ops.gl.p_glEnable(GL_STENCIL_TEST);
        checkGLcall("glEnable GL_STENCIL_TEST");

        if (gl_info->supported[WINED3D_GL_VERSION_2_0])
        {
            GL_EXTCALL(glStencilFuncSeparate(GL_FRONT, func, ref, mask));
            GL_EXTCALL(glStencilOpSeparate(GL_FRONT, stencilFail, depthFail, stencilPass));
            GL_EXTCALL(glStencilFuncSeparate(GL_BACK, func_back, ref, mask));
            GL_EXTCALL(glStencilOpSeparate(GL_BACK, stencilFail_back, depthFail_back, stencilPass_back));
            checkGLcall("setting two sided stencil state");
        }
        else if (gl_info->supported[EXT_STENCIL_TWO_SIDE])
        {
            /* Apply back first, then front. This function calls glActiveStencilFaceEXT,
             * which has an effect on the code below too. If we apply the front face
             * afterwards, we are sure that the active stencil face is set to front,
             * and other stencil functions which do not use two sided stencil do not have
             * to set it back
             */
            renderstate_stencil_twosided(context, GL_BACK,
                    func_back, ref, mask, stencilFail_back, depthFail_back, stencilPass_back);
            renderstate_stencil_twosided(context, GL_FRONT,
                    func, ref, mask, stencilFail, depthFail, stencilPass);
        }
        else if (gl_info->supported[ATI_SEPARATE_STENCIL])
        {
            GL_EXTCALL(glStencilFuncSeparateATI(func, func_back, ref, mask));
            checkGLcall("glStencilFuncSeparateATI(...)");
            GL_EXTCALL(glStencilOpSeparateATI(GL_FRONT, stencilFail, depthFail, stencilPass));
            checkGLcall("glStencilOpSeparateATI(GL_FRONT, ...)");
            GL_EXTCALL(glStencilOpSeparateATI(GL_BACK, stencilFail_back, depthFail_back, stencilPass_back));
            checkGLcall("glStencilOpSeparateATI(GL_BACK, ...)");
        }
        else
        {
            FIXME("Separate (two sided) stencil not supported on this version of OpenGL. Caps weren't honored?\n");
        }
    }
    else
    {
        if (gl_info->supported[EXT_STENCIL_TWO_SIDE])
        {
            gl_info->gl_ops.gl.p_glDisable(GL_STENCIL_TEST_TWO_SIDE_EXT);
            checkGLcall("glDisable(GL_STENCIL_TEST_TWO_SIDE_EXT)");
        }

        /* This code disables the ATI extension as well, since the standard stencil functions are equal
         * to calling the ATI functions with GL_FRONT_AND_BACK as face parameter
         */
        gl_info->gl_ops.gl.p_glEnable(GL_STENCIL_TEST);
        checkGLcall("glEnable GL_STENCIL_TEST");
        gl_info->gl_ops.gl.p_glStencilFunc(func, ref, mask);
        checkGLcall("glStencilFunc(...)");
        gl_info->gl_ops.gl.p_glStencilOp(stencilFail, depthFail, stencilPass);
        checkGLcall("glStencilOp(...)");
    }
}

static void depth(struct wined3d_context *context, const struct wined3d_state *state)
{
    const struct wined3d_gl_info *gl_info = wined3d_context_gl(context)->gl_info;
    const struct wined3d_depth_stencil_state *d = state->depth_stencil_state;
    BOOL enable_depth = d ? d->desc.depth : TRUE;
    GLenum depth_func = GL_LESS;

    if (!state->fb.depth_stencil)
    {
        TRACE("No depth/stencil buffer is attached; disabling depth test.\n");
        enable_depth = FALSE;
    }

    if (enable_depth)
    {
        gl_info->gl_ops.gl.p_glEnable(GL_DEPTH_TEST);
        checkGLcall("glEnable GL_DEPTH_TEST");
    }
    else
    {
        gl_info->gl_ops.gl.p_glDisable(GL_DEPTH_TEST);
        checkGLcall("glDisable GL_DEPTH_TEST");
    }

    if (!d || d->desc.depth_write)
    {
        gl_info->gl_ops.gl.p_glDepthMask(GL_TRUE);
        checkGLcall("glDepthMask(GL_TRUE)");
    }
    else
    {
        gl_info->gl_ops.gl.p_glDepthMask(GL_FALSE);
        checkGLcall("glDepthMask(GL_FALSE)");
    }

    if (d)
        depth_func = wined3d_gl_compare_func(d->desc.depth_func);
    if (depth_func)
    {
        gl_info->gl_ops.gl.p_glDepthFunc(depth_func);
        checkGLcall("glDepthFunc");
    }

    if (gl_info->supported[EXT_DEPTH_BOUNDS_TEST])
    {
        /* If min is larger than max, an INVALID_VALUE error is generated.
         * In d3d9, the test is not performed in this case. */
        if (state->depth_bounds_enable && state->depth_bounds_min <= state->depth_bounds_max)
        {
            gl_info->gl_ops.gl.p_glEnable(GL_DEPTH_BOUNDS_TEST_EXT);
            checkGLcall("glEnable(GL_DEPTH_BOUNDS_TEST_EXT)");
            GL_EXTCALL(glDepthBoundsEXT(state->depth_bounds_min, state->depth_bounds_max));
            checkGLcall("glDepthBoundsEXT");
        }
        else
        {
            gl_info->gl_ops.gl.p_glDisable(GL_DEPTH_BOUNDS_TEST_EXT);
            checkGLcall("glDisable(GL_DEPTH_BOUNDS_TEST_EXT)");
        }
    }

    if (context->stream_info.position_transformed && !isStateDirty(context, STATE_TRANSFORM(WINED3D_TS_PROJECTION)))
        context_apply_state(context, state, STATE_TRANSFORM(WINED3D_TS_PROJECTION));
}

static void depth_stencil(struct wined3d_context *context, const struct wined3d_state *state, DWORD state_id)
{
    const struct wined3d_gl_info *gl_info = wined3d_context_gl(context)->gl_info;
    const struct wined3d_depth_stencil_state *d = state->depth_stencil_state;
    GLuint stencil_write_mask = 0;

    depth(context, state);
    state_stencil(context, state);

    if (state->fb.depth_stencil)
        stencil_write_mask = d ? d->desc.stencil_write_mask : ~0u;

    gl_info->gl_ops.gl.p_glStencilMask(stencil_write_mask);
    checkGLcall("glStencilMask");
}

static void depth_stencil_2s(struct wined3d_context *context, const struct wined3d_state *state, DWORD state_id)
{
    const struct wined3d_gl_info *gl_info = wined3d_context_gl(context)->gl_info;
    const struct wined3d_depth_stencil_state *d = state->depth_stencil_state;
    GLuint stencil_write_mask = 0;

    depth(context, state);
    state_stencil(context, state);

    if (state->fb.depth_stencil)
        stencil_write_mask = d ? d->desc.stencil_write_mask : ~0u;

    GL_EXTCALL(glActiveStencilFaceEXT(GL_BACK));
    checkGLcall("glActiveStencilFaceEXT(GL_BACK)");
    gl_info->gl_ops.gl.p_glStencilMask(stencil_write_mask);
    checkGLcall("glStencilMask");
    GL_EXTCALL(glActiveStencilFaceEXT(GL_FRONT));
    checkGLcall("glActiveStencilFaceEXT(GL_FRONT)");
    gl_info->gl_ops.gl.p_glStencilMask(stencil_write_mask);
}

void state_fogstartend(struct wined3d_context *context, const struct wined3d_state *state, DWORD state_id)
{
    const struct wined3d_gl_info *gl_info = wined3d_context_gl(context)->gl_info;
    float fogstart, fogend;

    get_fog_start_end(context, state, &fogstart, &fogend);

    gl_info->gl_ops.gl.p_glFogf(GL_FOG_START, fogstart);
    checkGLcall("glFogf(GL_FOG_START, fogstart)");
    TRACE("Fog Start == %f\n", fogstart);

    gl_info->gl_ops.gl.p_glFogf(GL_FOG_END, fogend);
    checkGLcall("glFogf(GL_FOG_END, fogend)");
    TRACE("Fog End == %f\n", fogend);
}

void state_fog_fragpart(struct wined3d_context *context, const struct wined3d_state *state, DWORD state_id)
{
    const struct wined3d_gl_info *gl_info = wined3d_context_gl(context)->gl_info;
    enum fogsource new_source;
    DWORD fogstart = state->render_states[WINED3D_RS_FOGSTART];
    DWORD fogend = state->render_states[WINED3D_RS_FOGEND];

    TRACE("context %p, state %p, state_id %#lx.\n", context, state, state_id);

    if (!state->render_states[WINED3D_RS_FOGENABLE])
    {
        /* No fog? Disable it, and we're done :-) */
        gl_info->p_glDisableWINE(GL_FOG);
        checkGLcall("glDisable GL_FOG");
        return;
    }

    /* Fog Rules:
     *
     * With fixed function vertex processing, Direct3D knows 2 different fog input sources.
     * It can use the Z value of the vertex, or the alpha component of the specular color.
     * This depends on the fog vertex, fog table and the vertex declaration. If the Z value
     * is used, fogstart, fogend and the equation type are used, otherwise linear fog with
     * start = 255, end = 0 is used. Obviously the msdn is not very clear on that.
     *
     * FOGTABLEMODE != NONE:
     *  The Z value is used, with the equation specified, no matter what vertex type.
     *
     * FOGTABLEMODE == NONE, FOGVERTEXMODE != NONE, untransformed:
     *  Per vertex fog is calculated using the specified fog equation and the parameters
     *
     * FOGTABLEMODE == NONE, FOGVERTEXMODE != NONE, transformed, OR
     * FOGTABLEMODE == NONE, FOGVERTEXMODE == NONE, untransformed:
     *  Linear fog with start = 255.0, end = 0.0, input comes from the specular color
     *
     *
     * Rules for vertex fog with shaders:
     *
     * When mixing fixed function functionality with the programmable pipeline, D3D expects
     * the fog computation to happen during transformation while openGL expects it to happen
     * during rasterization. Also, prior to pixel shader 3.0 D3D handles fog blending after
     * the pixel shader while openGL always expects the pixel shader to handle the blending.
     * To solve this problem, WineD3D does:
     * 1) implement a linear fog equation and fog blending at the end of every pre 3.0 pixel
     * shader,
     * and 2) disables the fog computation (in either the fixed function or programmable
     * rasterizer) if using a vertex program.
     *
     * D3D shaders can provide an explicit fog coordinate. This fog coordinate is used with
     * D3DRS_FOGTABLEMODE==D3DFOG_NONE. The FOGVERTEXMODE is ignored, d3d always uses linear
     * fog with start=1.0 and end=0.0 in this case. This is similar to fog coordinates in
     * the specular color, a vertex shader counts as pretransformed geometry in this case.
     * There are some GL differences between specular fog coords and vertex shaders though.
     *
     * With table fog the vertex shader fog coordinate is ignored.
     *
     * If a fogtablemode and a fogvertexmode are specified, table fog is applied (with or
     * without shaders).
     */

    /* DX 7 sdk: "If both render states(vertex and table fog) are set to valid modes,
     * the system will apply only pixel(=table) fog effects."
     */
    if (state->render_states[WINED3D_RS_FOGTABLEMODE] == WINED3D_FOG_NONE)
    {
        if (use_vs(state))
        {
            gl_info->gl_ops.gl.p_glFogi(GL_FOG_MODE, GL_LINEAR);
            checkGLcall("glFogi(GL_FOG_MODE, GL_LINEAR)");
            new_source = FOGSOURCE_VS;
        }
        else
        {
            switch (state->render_states[WINED3D_RS_FOGVERTEXMODE])
            {
                /* If processed vertices are used, fall through to the NONE case */
                case WINED3D_FOG_EXP:
                    if (!context->stream_info.position_transformed)
                    {
                        gl_info->gl_ops.gl.p_glFogi(GL_FOG_MODE, GL_EXP);
                        checkGLcall("glFogi(GL_FOG_MODE, GL_EXP)");
                        new_source = FOGSOURCE_FFP;
                        break;
                    }
                    /* drop through */

                case WINED3D_FOG_EXP2:
                    if (!context->stream_info.position_transformed)
                    {
                        gl_info->gl_ops.gl.p_glFogi(GL_FOG_MODE, GL_EXP2);
                        checkGLcall("glFogi(GL_FOG_MODE, GL_EXP2)");
                        new_source = FOGSOURCE_FFP;
                        break;
                    }
                    /* drop through */

                case WINED3D_FOG_LINEAR:
                    if (!context->stream_info.position_transformed)
                    {
                        gl_info->gl_ops.gl.p_glFogi(GL_FOG_MODE, GL_LINEAR);
                        checkGLcall("glFogi(GL_FOG_MODE, GL_LINEAR)");
                        new_source = FOGSOURCE_FFP;
                        break;
                    }
                    /* drop through */

                case WINED3D_FOG_NONE:
                    /* Both are none? According to msdn the alpha channel of
                     * the specular colour contains a fog factor. Set it in
                     * draw_primitive_immediate_mode(). Same happens with
                     * vertex fog on transformed vertices. */
                    new_source = FOGSOURCE_COORD;
                    gl_info->gl_ops.gl.p_glFogi(GL_FOG_MODE, GL_LINEAR);
                    checkGLcall("glFogi(GL_FOG_MODE, GL_LINEAR)");
                    break;

                default:
                    FIXME("Unexpected WINED3D_RS_FOGVERTEXMODE %#x.\n",
                            state->render_states[WINED3D_RS_FOGVERTEXMODE]);
                    new_source = FOGSOURCE_FFP; /* Make the compiler happy */
            }
        }
    } else {
        new_source = FOGSOURCE_FFP;

        switch (state->render_states[WINED3D_RS_FOGTABLEMODE])
        {
            case WINED3D_FOG_EXP:
                gl_info->gl_ops.gl.p_glFogi(GL_FOG_MODE, GL_EXP);
                checkGLcall("glFogi(GL_FOG_MODE, GL_EXP)");
                break;

            case WINED3D_FOG_EXP2:
                gl_info->gl_ops.gl.p_glFogi(GL_FOG_MODE, GL_EXP2);
                checkGLcall("glFogi(GL_FOG_MODE, GL_EXP2)");
                break;

            case WINED3D_FOG_LINEAR:
                gl_info->gl_ops.gl.p_glFogi(GL_FOG_MODE, GL_LINEAR);
                checkGLcall("glFogi(GL_FOG_MODE, GL_LINEAR)");
                break;

            case WINED3D_FOG_NONE:   /* Won't happen */
            default:
                FIXME("Unexpected WINED3D_RS_FOGTABLEMODE %#x.\n",
                        state->render_states[WINED3D_RS_FOGTABLEMODE]);
        }
    }

    gl_info->p_glEnableWINE(GL_FOG);
    checkGLcall("glEnable GL_FOG");
    if (new_source != context->fog_source || fogstart == fogend)
    {
        context->fog_source = new_source;
        state_fogstartend(context, state, STATE_RENDER(WINED3D_RS_FOGSTART));
    }
}

void state_fogcolor(struct wined3d_context *context, const struct wined3d_state *state, DWORD state_id)
{
    const struct wined3d_gl_info *gl_info = wined3d_context_gl(context)->gl_info;
    struct wined3d_color color;

    wined3d_color_from_d3dcolor(&color, state->render_states[WINED3D_RS_FOGCOLOR]);
    gl_info->gl_ops.gl.p_glFogfv(GL_FOG_COLOR, &color.r);
    checkGLcall("glFog GL_FOG_COLOR");
}

void state_fogdensity(struct wined3d_context *context, const struct wined3d_state *state, DWORD state_id)
{
    const struct wined3d_gl_info *gl_info = wined3d_context_gl(context)->gl_info;
    union {
        DWORD d;
        float f;
    } tmpvalue;

    tmpvalue.d = state->render_states[WINED3D_RS_FOGDENSITY];
    gl_info->gl_ops.gl.p_glFogfv(GL_FOG_DENSITY, &tmpvalue.f);
    checkGLcall("glFogf(GL_FOG_DENSITY, (float) Value)");
}

/* Activates the texture dimension according to the bound D3D texture. Does
 * not care for the colorop or correct gl texture unit (when using nvrc).
 * Requires the caller to activate the correct unit. */
/* Context activation is done by the caller (state handler). */
void texture_activate_dimensions(struct wined3d_texture *texture, const struct wined3d_gl_info *gl_info)
{
    if (texture)
    {
        switch (wined3d_texture_gl(texture)->target)
        {
            case GL_TEXTURE_2D:
                gl_info->gl_ops.gl.p_glDisable(GL_TEXTURE_3D);
                checkGLcall("glDisable(GL_TEXTURE_3D)");
                if (gl_info->supported[ARB_TEXTURE_CUBE_MAP])
                {
                    gl_info->gl_ops.gl.p_glDisable(GL_TEXTURE_CUBE_MAP_ARB);
                    checkGLcall("glDisable(GL_TEXTURE_CUBE_MAP_ARB)");
                }
                if (gl_info->supported[ARB_TEXTURE_RECTANGLE])
                {
                    gl_info->gl_ops.gl.p_glDisable(GL_TEXTURE_RECTANGLE_ARB);
                    checkGLcall("glDisable(GL_TEXTURE_RECTANGLE_ARB)");
                }
                gl_info->gl_ops.gl.p_glEnable(GL_TEXTURE_2D);
                checkGLcall("glEnable(GL_TEXTURE_2D)");
                break;

            case GL_TEXTURE_RECTANGLE_ARB:
                gl_info->gl_ops.gl.p_glDisable(GL_TEXTURE_2D);
                checkGLcall("glDisable(GL_TEXTURE_2D)");
                gl_info->gl_ops.gl.p_glDisable(GL_TEXTURE_3D);
                checkGLcall("glDisable(GL_TEXTURE_3D)");
                if (gl_info->supported[ARB_TEXTURE_CUBE_MAP])
                {
                    gl_info->gl_ops.gl.p_glDisable(GL_TEXTURE_CUBE_MAP_ARB);
                    checkGLcall("glDisable(GL_TEXTURE_CUBE_MAP_ARB)");
                }
                gl_info->gl_ops.gl.p_glEnable(GL_TEXTURE_RECTANGLE_ARB);
                checkGLcall("glEnable(GL_TEXTURE_RECTANGLE_ARB)");
                break;

            case GL_TEXTURE_3D:
                if (gl_info->supported[ARB_TEXTURE_CUBE_MAP])
                {
                    gl_info->gl_ops.gl.p_glDisable(GL_TEXTURE_CUBE_MAP_ARB);
                    checkGLcall("glDisable(GL_TEXTURE_CUBE_MAP_ARB)");
                }
                if (gl_info->supported[ARB_TEXTURE_RECTANGLE])
                {
                    gl_info->gl_ops.gl.p_glDisable(GL_TEXTURE_RECTANGLE_ARB);
                    checkGLcall("glDisable(GL_TEXTURE_RECTANGLE_ARB)");
                }
                gl_info->gl_ops.gl.p_glDisable(GL_TEXTURE_2D);
                checkGLcall("glDisable(GL_TEXTURE_2D)");
                gl_info->gl_ops.gl.p_glEnable(GL_TEXTURE_3D);
                checkGLcall("glEnable(GL_TEXTURE_3D)");
                break;

            case GL_TEXTURE_CUBE_MAP_ARB:
                gl_info->gl_ops.gl.p_glDisable(GL_TEXTURE_2D);
                checkGLcall("glDisable(GL_TEXTURE_2D)");
                gl_info->gl_ops.gl.p_glDisable(GL_TEXTURE_3D);
                checkGLcall("glDisable(GL_TEXTURE_3D)");
                if (gl_info->supported[ARB_TEXTURE_RECTANGLE])
                {
                    gl_info->gl_ops.gl.p_glDisable(GL_TEXTURE_RECTANGLE_ARB);
                    checkGLcall("glDisable(GL_TEXTURE_RECTANGLE_ARB)");
                }
                gl_info->gl_ops.gl.p_glEnable(GL_TEXTURE_CUBE_MAP_ARB);
                checkGLcall("glEnable(GL_TEXTURE_CUBE_MAP_ARB)");
                break;
        }
    }
    else
    {
        gl_info->gl_ops.gl.p_glEnable(GL_TEXTURE_2D);
        checkGLcall("glEnable(GL_TEXTURE_2D)");
        gl_info->gl_ops.gl.p_glDisable(GL_TEXTURE_3D);
        checkGLcall("glDisable(GL_TEXTURE_3D)");
        if (gl_info->supported[ARB_TEXTURE_CUBE_MAP])
        {
            gl_info->gl_ops.gl.p_glDisable(GL_TEXTURE_CUBE_MAP_ARB);
            checkGLcall("glDisable(GL_TEXTURE_CUBE_MAP_ARB)");
        }
        if (gl_info->supported[ARB_TEXTURE_RECTANGLE])
        {
            gl_info->gl_ops.gl.p_glDisable(GL_TEXTURE_RECTANGLE_ARB);
            checkGLcall("glDisable(GL_TEXTURE_RECTANGLE_ARB)");
        }
        /* Binding textures is done by samplers. A dummy texture will be bound. */
    }
}

/* Context activation is done by the caller (state handler). */
void sampler_texdim(struct wined3d_context *context, const struct wined3d_state *state, DWORD state_id)
{
    struct wined3d_context_gl *context_gl = wined3d_context_gl(context);
    unsigned int sampler = state_id - STATE_SAMPLER(0);
    unsigned int mapped_stage;

    /* No need to enable / disable anything here for unused samplers. The
     * tex_colorop handler takes care. Also no action is needed with pixel
     * shaders, or if tex_colorop will take care of this business. */
    mapped_stage = context_gl->tex_unit_map[sampler];
    if (mapped_stage == WINED3D_UNMAPPED_STAGE || mapped_stage >= context_gl->gl_info->limits.ffp_textures)
        return;
    if (sampler >= context->lowest_disabled_stage)
        return;
    if (isStateDirty(context, STATE_TEXTURESTAGE(sampler, WINED3D_TSS_COLOR_OP)))
        return;

    wined3d_context_gl_active_texture(context_gl, context_gl->gl_info, sampler);
    texture_activate_dimensions(wined3d_state_get_ffp_texture(state, sampler), context_gl->gl_info);
}

static void state_linepattern(struct wined3d_context *context, const struct wined3d_state *state, DWORD state_id)
{
    const struct wined3d_gl_info *gl_info = wined3d_context_gl(context)->gl_info;
    union
    {
        DWORD d;
        struct wined3d_line_pattern lp;
    } tmppattern;
    tmppattern.d = state->render_states[WINED3D_RS_LINEPATTERN];

    TRACE("Line pattern: repeat %d bits %x.\n", tmppattern.lp.repeat_factor, tmppattern.lp.line_pattern);

    if (tmppattern.lp.repeat_factor)
    {
        gl_info->gl_ops.gl.p_glLineStipple(tmppattern.lp.repeat_factor, tmppattern.lp.line_pattern);
        checkGLcall("glLineStipple(repeat, linepattern)");
        gl_info->gl_ops.gl.p_glEnable(GL_LINE_STIPPLE);
        checkGLcall("glEnable(GL_LINE_STIPPLE);");
    }
    else
    {
        gl_info->gl_ops.gl.p_glDisable(GL_LINE_STIPPLE);
        checkGLcall("glDisable(GL_LINE_STIPPLE);");
    }
}

static void state_linepattern_w(struct wined3d_context *context, const struct wined3d_state *state, DWORD state_id)
{
    static unsigned int once;

    if (!once++)
        FIXME("Setting line patterns is not supported in OpenGL core contexts.\n");
}

void state_pointsprite_w(struct wined3d_context *context, const struct wined3d_state *state, DWORD state_id)
{
    static BOOL warned;

    /* TODO: NV_POINT_SPRITE */
    if (!warned && state->render_states[WINED3D_RS_POINTSPRITEENABLE])
    {
        /* A FIXME, not a WARN because point sprites should be software emulated if not supported by HW */
        FIXME("Point sprites not supported\n");
        warned = TRUE;
    }
}

void state_pointsprite(struct wined3d_context *context, const struct wined3d_state *state, DWORD state_id)
{
    const struct wined3d_gl_info *gl_info = wined3d_context_gl(context)->gl_info;

    if (state->render_states[WINED3D_RS_POINTSPRITEENABLE])
    {
        gl_info->gl_ops.gl.p_glEnable(GL_POINT_SPRITE_ARB);
        checkGLcall("glEnable(GL_POINT_SPRITE_ARB)");
    }
    else
    {
        gl_info->gl_ops.gl.p_glDisable(GL_POINT_SPRITE_ARB);
        checkGLcall("glDisable(GL_POINT_SPRITE_ARB)");
    }
}

static void state_msaa_w(struct wined3d_context *context, const struct wined3d_state *state, DWORD state_id)
{
    if (state->render_states[WINED3D_RS_MULTISAMPLEANTIALIAS])
        WARN("Multisample antialiasing not supported by GL.\n");
}

static void state_msaa(struct wined3d_context *context, const struct wined3d_state *state, DWORD state_id)
{
    const struct wined3d_gl_info *gl_info = wined3d_context_gl(context)->gl_info;

    if (state->render_states[WINED3D_RS_MULTISAMPLEANTIALIAS])
    {
        gl_info->gl_ops.gl.p_glEnable(GL_MULTISAMPLE_ARB);
        checkGLcall("glEnable(GL_MULTISAMPLE_ARB)");
    }
    else
    {
        gl_info->gl_ops.gl.p_glDisable(GL_MULTISAMPLE_ARB);
        checkGLcall("glDisable(GL_MULTISAMPLE_ARB)");
    }
}

static void line_antialias(const struct wined3d_rasterizer_state *r, const struct wined3d_gl_info *gl_info)
{
    if (r && r->desc.line_antialias)
    {
        gl_info->gl_ops.gl.p_glEnable(GL_LINE_SMOOTH);
        checkGLcall("glEnable(GL_LINE_SMOOTH)");
    }
    else
    {
        gl_info->gl_ops.gl.p_glDisable(GL_LINE_SMOOTH);
        checkGLcall("glDisable(GL_LINE_SMOOTH)");
    }
}

static void scissor(const struct wined3d_rasterizer_state *r, const struct wined3d_gl_info *gl_info)
{
    if (r && r->desc.scissor)
    {
        gl_info->gl_ops.gl.p_glEnable(GL_SCISSOR_TEST);
        checkGLcall("glEnable(GL_SCISSOR_TEST)");
    }
    else
    {
        gl_info->gl_ops.gl.p_glDisable(GL_SCISSOR_TEST);
        checkGLcall("glDisable(GL_SCISSOR_TEST)");
    }
}

/* The Direct3D depth bias is specified in normalized depth coordinates. In
 * OpenGL the bias is specified in units of "the smallest value that is
 * guaranteed to produce a resolvable offset for a given implementation". To
 * convert from D3D to GL we need to divide the D3D depth bias by that value.
 * We try to detect the value from GL with test draws. On most drivers (r300g,
 * 600g, Nvidia, i965 on Mesa) the value is 2^23 for fixed point depth buffers,
 * for r200 and i965 on OSX it is 2^24, for r500 on OSX it is 2^22. For floating
 * point buffers it is 2^22, 2^23 or 2^24 depending on the GPU. The value does
 * not depend on the depth buffer precision on any driver.
 *
 * Two games that are picky regarding depth bias are Mass Effect 2 (flickering
 * decals) and F.E.A.R and F.E.A.R. 2 (semi-transparent guns).
 *
 * Note that SLOPESCALEDEPTHBIAS is a scaling factor for the depth slope, and
 * doesn't need to be scaled to account for GL vs D3D differences. */
static void depthbias(struct wined3d_context *context, const struct wined3d_state *state)
{
    const struct wined3d_gl_info *gl_info = wined3d_context_gl(context)->gl_info;
    const struct wined3d_rasterizer_state *r = state->rasterizer_state;
    float scale_bias = r ? r->desc.scale_bias : 0.0f;
    union
    {
        DWORD d;
        float f;
    } const_bias;

    const_bias.f = r ? r->desc.depth_bias : 0.0f;

    if (scale_bias || const_bias.f)
    {
        const struct wined3d_rendertarget_view *depth = state->fb.depth_stencil;
        float factor, units, scale, clamp;

        clamp = r ? r->desc.depth_bias_clamp : 0.0f;

        if (context->d3d_info->wined3d_creation_flags & WINED3D_LEGACY_DEPTH_BIAS)
        {
            factor = units = -(float)const_bias.d;
        }
        else
        {
            if (depth)
            {
                scale = depth->format->depth_bias_scale;

                TRACE("Depth format %s, using depthbias scale of %.8e.\n",
                        debug_d3dformat(depth->format->id), scale);
            }
            else
            {
                /* The context manager will reapply this state on a depth stencil change */
                TRACE("No depth stencil, using depth bias scale of 0.0.\n");
                scale = 0.0f;
            }

            factor = scale_bias;
            units = const_bias.f * scale;
        }

        gl_info->gl_ops.gl.p_glEnable(GL_POLYGON_OFFSET_FILL);
        if (gl_info->supported[ARB_POLYGON_OFFSET_CLAMP])
        {
            gl_info->gl_ops.ext.p_glPolygonOffsetClamp(factor, units, clamp);
        }
        else
        {
            if (clamp != 0.0f)
                WARN("Ignoring depth bias clamp %.8e.\n", clamp);
            gl_info->gl_ops.gl.p_glPolygonOffset(factor, units);
        }
    }
    else
    {
        gl_info->gl_ops.gl.p_glDisable(GL_POLYGON_OFFSET_FILL);
    }

    checkGLcall("depth bias");
}

static void state_sample_mask(struct wined3d_context *context, const struct wined3d_state *state, DWORD state_id)
{
    const struct wined3d_gl_info *gl_info = wined3d_context_gl(context)->gl_info;
    unsigned int sample_mask = state->sample_mask;

    TRACE("Setting sample mask to %#x.\n", sample_mask);
    if (sample_mask != 0xffffffff)
    {
        gl_info->gl_ops.gl.p_glEnable(GL_SAMPLE_MASK);
        checkGLcall("glEnable GL_SAMPLE_MASK");
        GL_EXTCALL(glSampleMaski(0, sample_mask));
        checkGLcall("glSampleMaski");
    }
    else
    {
        gl_info->gl_ops.gl.p_glDisable(GL_SAMPLE_MASK);
        checkGLcall("glDisable GL_SAMPLE_MASK");
    }
}

static void state_sample_mask_w(struct wined3d_context *context, const struct wined3d_state *state, DWORD state_id)
{
    WARN("Unsupported in local OpenGL implementation: glSampleMaski.\n");
}

static void get_src_and_opr(uint32_t arg, BOOL is_alpha, GLenum* source, GLenum* operand) {
    /* The WINED3DTA_ALPHAREPLICATE flag specifies the alpha component of the
    * input should be used for all input components. The WINED3DTA_COMPLEMENT
    * flag specifies the complement of the input should be used. */
    BOOL from_alpha = is_alpha || arg & WINED3DTA_ALPHAREPLICATE;
    BOOL complement = arg & WINED3DTA_COMPLEMENT;

    /* Calculate the operand */
    if (complement) {
        if (from_alpha) *operand = GL_ONE_MINUS_SRC_ALPHA;
        else *operand = GL_ONE_MINUS_SRC_COLOR;
    } else {
        if (from_alpha) *operand = GL_SRC_ALPHA;
        else *operand = GL_SRC_COLOR;
    }

    /* Calculate the source */
    switch (arg & WINED3DTA_SELECTMASK) {
        case WINED3DTA_CURRENT: *source = GL_PREVIOUS_EXT; break;
        case WINED3DTA_DIFFUSE: *source = GL_PRIMARY_COLOR_EXT; break;
        case WINED3DTA_TEXTURE: *source = GL_TEXTURE; break;
        case WINED3DTA_TFACTOR: *source = GL_CONSTANT_EXT; break;
        case WINED3DTA_SPECULAR:
            /*
            * According to the GL_ARB_texture_env_combine specs, SPECULAR is
            * 'Secondary color' and isn't supported until base GL supports it
            * There is no concept of temp registers as far as I can tell
            */
            FIXME("Unhandled texture arg WINED3DTA_SPECULAR\n");
            *source = GL_TEXTURE;
            break;
        default:
            FIXME("Unrecognized texture arg %#x\n", arg);
            *source = GL_TEXTURE;
            break;
    }
}

/* Setup the texture operations texture stage states */
static void set_tex_op(const struct wined3d_gl_info *gl_info, const struct wined3d_state *state,
        BOOL isAlpha, int Stage, enum wined3d_texture_op op, uint32_t arg1, uint32_t arg2, uint32_t arg3)
{
    GLenum src1, src2, src3;
    GLenum opr1, opr2, opr3;
    GLenum comb_target;
    GLenum src0_target, src1_target, src2_target;
    GLenum opr0_target, opr1_target, opr2_target;
    GLenum scal_target;
    GLenum opr=0, invopr, src3_target, opr3_target;
    BOOL Handled = FALSE;

    TRACE("Alpha?(%d), Stage:%d Op(%s), a1(%d), a2(%d), a3(%d)\n", isAlpha, Stage, debug_d3dtop(op), arg1, arg2, arg3);

    /* Operations usually involve two args, src0 and src1 and are operations
     * of the form (a1 <operation> a2). However, some of the more complex
     * operations take 3 parameters. Instead of the (sensible) addition of a3,
     * Microsoft added in a third parameter called a0. Therefore these are
     * operations of the form a0 <operation> a1 <operation> a2. I.e., the new
     * parameter goes to the front.
     *
     * However, below we treat the new (a0) parameter as src2/opr2, so in the
     * actual functions below, expect their syntax to differ slightly to those
     * listed in the manuals. I.e., replace arg1 with arg3, arg2 with arg1 and
     * arg3 with arg2. This affects WINED3DTOP_MULTIPLYADD and WINED3DTOP_LERP. */

    if (isAlpha)
    {
        comb_target = GL_COMBINE_ALPHA;
        src0_target = GL_SOURCE0_ALPHA;
        src1_target = GL_SOURCE1_ALPHA;
        src2_target = GL_SOURCE2_ALPHA;
        opr0_target = GL_OPERAND0_ALPHA;
        opr1_target = GL_OPERAND1_ALPHA;
        opr2_target = GL_OPERAND2_ALPHA;
        scal_target = GL_ALPHA_SCALE;
    }
    else
    {
        comb_target = GL_COMBINE_RGB;
        src0_target = GL_SOURCE0_RGB;
        src1_target = GL_SOURCE1_RGB;
        src2_target = GL_SOURCE2_RGB;
        opr0_target = GL_OPERAND0_RGB;
        opr1_target = GL_OPERAND1_RGB;
        opr2_target = GL_OPERAND2_RGB;
        scal_target = GL_RGB_SCALE;
    }

        /* If a texture stage references an invalid texture unit the stage just
        * passes through the result from the previous stage */
    if (is_invalid_op(state, Stage, op, arg1, arg2, arg3))
    {
        arg1 = WINED3DTA_CURRENT;
        op = WINED3D_TOP_SELECT_ARG1;
    }

    if (isAlpha && !wined3d_state_get_ffp_texture(state, Stage) && arg1 == WINED3DTA_TEXTURE)
    {
        get_src_and_opr(WINED3DTA_DIFFUSE, isAlpha, &src1, &opr1);
    } else {
        get_src_and_opr(arg1, isAlpha, &src1, &opr1);
    }
    get_src_and_opr(arg2, isAlpha, &src2, &opr2);
    get_src_and_opr(arg3, isAlpha, &src3, &opr3);

    TRACE("ct(%x), 1:(%x,%x), 2:(%x,%x), 3:(%x,%x)\n", comb_target, src1, opr1, src2, opr2, src3, opr3);

    Handled = TRUE; /* Assume will be handled */

    /* Other texture operations require special extensions: */
    if (gl_info->supported[NV_TEXTURE_ENV_COMBINE4])
    {
        if (isAlpha) {
            opr = GL_SRC_ALPHA;
            invopr = GL_ONE_MINUS_SRC_ALPHA;
            src3_target = GL_SOURCE3_ALPHA_NV;
            opr3_target = GL_OPERAND3_ALPHA_NV;
        } else {
            opr = GL_SRC_COLOR;
            invopr = GL_ONE_MINUS_SRC_COLOR;
            src3_target = GL_SOURCE3_RGB_NV;
            opr3_target = GL_OPERAND3_RGB_NV;
        }
        switch (op)
        {
            case WINED3D_TOP_DISABLE: /* Only for alpha */
                gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, comb_target, GL_ADD);
                checkGLcall("GL_TEXTURE_ENV, comb_target, GL_REPLACE");
                gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, src0_target, GL_PREVIOUS_EXT);
                checkGLcall("GL_TEXTURE_ENV, src0_target, src1");
                gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, opr0_target, GL_SRC_ALPHA);
                checkGLcall("GL_TEXTURE_ENV, opr0_target, opr1");
                gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, src1_target, GL_ZERO);
                checkGLcall("GL_TEXTURE_ENV, src1_target, GL_ZERO");
                gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, opr1_target, invopr);
                checkGLcall("GL_TEXTURE_ENV, opr1_target, invopr");
                gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, src2_target, GL_ZERO);
                checkGLcall("GL_TEXTURE_ENV, src2_target, GL_ZERO");
                gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, opr2_target, opr);
                checkGLcall("GL_TEXTURE_ENV, opr2_target, opr");
                gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, src3_target, GL_ZERO);
                checkGLcall("GL_TEXTURE_ENV, src3_target, GL_ZERO");
                gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, opr3_target, opr);
                checkGLcall("GL_TEXTURE_ENV, opr3_target, opr");
                break;

            case WINED3D_TOP_SELECT_ARG1: /* = a1 * 1 + 0 * 0 */
            case WINED3D_TOP_SELECT_ARG2: /* = a2 * 1 + 0 * 0 */
                gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, comb_target, GL_ADD);
                checkGLcall("GL_TEXTURE_ENV, comb_target, GL_ADD");
                if (op == WINED3D_TOP_SELECT_ARG1)
                {
                    gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, src0_target, src1);
                    checkGLcall("GL_TEXTURE_ENV, src0_target, src1");
                    gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, opr0_target, opr1);
                    checkGLcall("GL_TEXTURE_ENV, opr0_target, opr1");
                }
                else
                {
                    gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, src0_target, src2);
                    checkGLcall("GL_TEXTURE_ENV, src0_target, src2");
                    gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, opr0_target, opr2);
                    checkGLcall("GL_TEXTURE_ENV, opr0_target, opr2");
                }
                gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, src1_target, GL_ZERO);
                checkGLcall("GL_TEXTURE_ENV, src1_target, GL_ZERO");
                gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, opr1_target, invopr);
                checkGLcall("GL_TEXTURE_ENV, opr1_target, invopr");
                gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, src2_target, GL_ZERO);
                checkGLcall("GL_TEXTURE_ENV, src2_target, GL_ZERO");
                gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, opr2_target, opr);
                checkGLcall("GL_TEXTURE_ENV, opr2_target, opr");
                gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, src3_target, GL_ZERO);
                checkGLcall("GL_TEXTURE_ENV, src3_target, GL_ZERO");
                gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, opr3_target, opr);
                checkGLcall("GL_TEXTURE_ENV, opr3_target, opr");
                break;

            case WINED3D_TOP_MODULATE:
                gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, comb_target, GL_ADD);
                checkGLcall("GL_TEXTURE_ENV, comb_target, GL_ADD"); /* Add = a0*a1 + a2*a3 */
                gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, src0_target, src1);
                checkGLcall("GL_TEXTURE_ENV, src0_target, src1");
                gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, opr0_target, opr1);
                checkGLcall("GL_TEXTURE_ENV, opr0_target, opr1");
                gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, src1_target, src2);
                checkGLcall("GL_TEXTURE_ENV, src1_target, GL_ZERO");
                gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, opr1_target, opr2);
                checkGLcall("GL_TEXTURE_ENV, opr1_target, invopr");
                gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, src2_target, GL_ZERO);
                checkGLcall("GL_TEXTURE_ENV, src2_target, src2");
                gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, opr2_target, opr);
                checkGLcall("GL_TEXTURE_ENV, opr2_target, opr2");
                gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, src3_target, GL_ZERO);
                checkGLcall("GL_TEXTURE_ENV, src3_target, GL_ZERO");
                gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, opr3_target, opr);
                checkGLcall("GL_TEXTURE_ENV, opr3_target, opr1");
                gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, scal_target, 1);
                checkGLcall("GL_TEXTURE_ENV, scal_target, 1");
                break;
            case WINED3D_TOP_MODULATE_2X:
                gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, comb_target, GL_ADD);
                checkGLcall("GL_TEXTURE_ENV, comb_target, GL_ADD"); /* Add = a0*a1 + a2*a3 */
                gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, src0_target, src1);
                checkGLcall("GL_TEXTURE_ENV, src0_target, src1");
                gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, opr0_target, opr1);
                checkGLcall("GL_TEXTURE_ENV, opr0_target, opr1");
                gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, src1_target, src2);
                checkGLcall("GL_TEXTURE_ENV, src1_target, GL_ZERO");
                gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, opr1_target, opr2);
                checkGLcall("GL_TEXTURE_ENV, opr1_target, invopr");
                gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, src2_target, GL_ZERO);
                checkGLcall("GL_TEXTURE_ENV, src2_target, src2");
                gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, opr2_target, opr);
                checkGLcall("GL_TEXTURE_ENV, opr2_target, opr2");
                gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, src3_target, GL_ZERO);
                checkGLcall("GL_TEXTURE_ENV, src3_target, GL_ZERO");
                gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, opr3_target, opr);
                checkGLcall("GL_TEXTURE_ENV, opr3_target, opr1");
                gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, scal_target, 2);
                checkGLcall("GL_TEXTURE_ENV, scal_target, 2");
                break;
            case WINED3D_TOP_MODULATE_4X:
                gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, comb_target, GL_ADD);
                checkGLcall("GL_TEXTURE_ENV, comb_target, GL_ADD"); /* Add = a0*a1 + a2*a3 */
                gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, src0_target, src1);
                checkGLcall("GL_TEXTURE_ENV, src0_target, src1");
                gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, opr0_target, opr1);
                checkGLcall("GL_TEXTURE_ENV, opr0_target, opr1");
                gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, src1_target, src2);
                checkGLcall("GL_TEXTURE_ENV, src1_target, GL_ZERO");
                gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, opr1_target, opr2);
                checkGLcall("GL_TEXTURE_ENV, opr1_target, invopr");
                gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, src2_target, GL_ZERO);
                checkGLcall("GL_TEXTURE_ENV, src2_target, src2");
                gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, opr2_target, opr);
                checkGLcall("GL_TEXTURE_ENV, opr2_target, opr2");
                gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, src3_target, GL_ZERO);
                checkGLcall("GL_TEXTURE_ENV, src3_target, GL_ZERO");
                gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, opr3_target, opr);
                checkGLcall("GL_TEXTURE_ENV, opr3_target, opr1");
                gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, scal_target, 4);
                checkGLcall("GL_TEXTURE_ENV, scal_target, 4");
                break;

            case WINED3D_TOP_ADD:
                gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, comb_target, GL_ADD);
                checkGLcall("GL_TEXTURE_ENV, comb_target, GL_ADD");
                gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, src0_target, src1);
                checkGLcall("GL_TEXTURE_ENV, src0_target, src1");
                gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, opr0_target, opr1);
                checkGLcall("GL_TEXTURE_ENV, opr0_target, opr1");
                gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, src1_target, GL_ZERO);
                checkGLcall("GL_TEXTURE_ENV, src1_target, GL_ZERO");
                gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, opr1_target, invopr);
                checkGLcall("GL_TEXTURE_ENV, opr1_target, invopr");
                gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, src2_target, src2);
                checkGLcall("GL_TEXTURE_ENV, src2_target, src2");
                gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, opr2_target, opr2);
                checkGLcall("GL_TEXTURE_ENV, opr2_target, opr2");
                gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, src3_target, GL_ZERO);
                checkGLcall("GL_TEXTURE_ENV, src3_target, GL_ZERO");
                gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, opr3_target, invopr);
                checkGLcall("GL_TEXTURE_ENV, opr3_target, invopr");
                gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, scal_target, 1);
                checkGLcall("GL_TEXTURE_ENV, scal_target, 1");
                break;

            case WINED3D_TOP_ADD_SIGNED:
                gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, comb_target, GL_ADD_SIGNED);
                checkGLcall("GL_TEXTURE_ENV, comb_target, GL_ADD_SIGNED");
                gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, src0_target, src1);
                checkGLcall("GL_TEXTURE_ENV, src0_target, src1");
                gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, opr0_target, opr1);
                checkGLcall("GL_TEXTURE_ENV, opr0_target, opr1");
                gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, src1_target, GL_ZERO);
                checkGLcall("GL_TEXTURE_ENV, src1_target, GL_ZERO");
                gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, opr1_target, invopr);
                checkGLcall("GL_TEXTURE_ENV, opr1_target, invopr");
                gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, src2_target, src2);
                checkGLcall("GL_TEXTURE_ENV, src2_target, src2");
                gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, opr2_target, opr2);
                checkGLcall("GL_TEXTURE_ENV, opr2_target, opr2");
                gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, src3_target, GL_ZERO);
                checkGLcall("GL_TEXTURE_ENV, src3_target, GL_ZERO");
                gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, opr3_target, invopr);
                checkGLcall("GL_TEXTURE_ENV, opr3_target, invopr");
                gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, scal_target, 1);
                checkGLcall("GL_TEXTURE_ENV, scal_target, 1");
                break;

            case WINED3D_TOP_ADD_SIGNED_2X:
                gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, comb_target, GL_ADD_SIGNED);
                checkGLcall("GL_TEXTURE_ENV, comb_target, GL_ADD_SIGNED");
                gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, src0_target, src1);
                checkGLcall("GL_TEXTURE_ENV, src0_target, src1");
                gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, opr0_target, opr1);
                checkGLcall("GL_TEXTURE_ENV, opr0_target, opr1");
                gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, src1_target, GL_ZERO);
                checkGLcall("GL_TEXTURE_ENV, src1_target, GL_ZERO");
                gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, opr1_target, invopr);
                checkGLcall("GL_TEXTURE_ENV, opr1_target, invopr");
                gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, src2_target, src2);
                checkGLcall("GL_TEXTURE_ENV, src2_target, src2");
                gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, opr2_target, opr2);
                checkGLcall("GL_TEXTURE_ENV, opr2_target, opr2");
                gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, src3_target, GL_ZERO);
                checkGLcall("GL_TEXTURE_ENV, src3_target, GL_ZERO");
                gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, opr3_target, invopr);
                checkGLcall("GL_TEXTURE_ENV, opr3_target, invopr");
                gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, scal_target, 2);
                checkGLcall("GL_TEXTURE_ENV, scal_target, 2");
                break;

            case WINED3D_TOP_ADD_SMOOTH:
                gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, comb_target, GL_ADD);
                checkGLcall("GL_TEXTURE_ENV, comb_target, GL_ADD");
                gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, src0_target, src1);
                checkGLcall("GL_TEXTURE_ENV, src0_target, src1");
                gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, opr0_target, opr1);
                checkGLcall("GL_TEXTURE_ENV, opr0_target, opr1");
                gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, src1_target, GL_ZERO);
                checkGLcall("GL_TEXTURE_ENV, src1_target, GL_ZERO");
                gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, opr1_target, invopr);
                checkGLcall("GL_TEXTURE_ENV, opr1_target, invopr");
                gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, src2_target, src2);
                checkGLcall("GL_TEXTURE_ENV, src2_target, src2");
                gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, opr2_target, opr2);
                checkGLcall("GL_TEXTURE_ENV, opr2_target, opr2");
                gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, src3_target, src1);
                checkGLcall("GL_TEXTURE_ENV, src3_target, src1");
                switch (opr1) {
                    case GL_SRC_COLOR: opr = GL_ONE_MINUS_SRC_COLOR; break;
                    case GL_ONE_MINUS_SRC_COLOR: opr = GL_SRC_COLOR; break;
                    case GL_SRC_ALPHA: opr = GL_ONE_MINUS_SRC_ALPHA; break;
                    case GL_ONE_MINUS_SRC_ALPHA: opr = GL_SRC_ALPHA; break;
                }
                gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, opr3_target, opr);
                checkGLcall("GL_TEXTURE_ENV, opr3_target, opr");
                gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, scal_target, 1);
                checkGLcall("GL_TEXTURE_ENV, scal_target, 1");
                break;

            case WINED3D_TOP_BLEND_DIFFUSE_ALPHA:
                gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, comb_target, GL_ADD);
                checkGLcall("GL_TEXTURE_ENV, comb_target, GL_ADD");
                gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, src0_target, src1);
                checkGLcall("GL_TEXTURE_ENV, src0_target, src1");
                gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, opr0_target, opr1);
                checkGLcall("GL_TEXTURE_ENV, opr0_target, opr1");
                gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, src1_target, GL_PRIMARY_COLOR);
                checkGLcall("GL_TEXTURE_ENV, src1_target, GL_PRIMARY_COLOR");
                gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, opr1_target, invopr);
                checkGLcall("GL_TEXTURE_ENV, opr1_target, invopr");
                gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, src2_target, src2);
                checkGLcall("GL_TEXTURE_ENV, src2_target, src2");
                gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, opr2_target, opr2);
                checkGLcall("GL_TEXTURE_ENV, opr2_target, opr2");
                gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, src3_target, GL_PRIMARY_COLOR);
                checkGLcall("GL_TEXTURE_ENV, src3_target, GL_PRIMARY_COLOR");
                gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, opr3_target, GL_ONE_MINUS_SRC_ALPHA);
                checkGLcall("GL_TEXTURE_ENV, opr3_target, GL_ONE_MINUS_SRC_ALPHA");
                gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, scal_target, 1);
                checkGLcall("GL_TEXTURE_ENV, scal_target, 1");
                break;
            case WINED3D_TOP_BLEND_TEXTURE_ALPHA:
                gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, comb_target, GL_ADD);
                checkGLcall("GL_TEXTURE_ENV, comb_target, GL_ADD");
                gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, src0_target, src1);
                checkGLcall("GL_TEXTURE_ENV, src0_target, src1");
                gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, opr0_target, opr1);
                checkGLcall("GL_TEXTURE_ENV, opr0_target, opr1");
                gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, src1_target, GL_TEXTURE);
                checkGLcall("GL_TEXTURE_ENV, src1_target, GL_TEXTURE");
                gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, opr1_target, invopr);
                checkGLcall("GL_TEXTURE_ENV, opr1_target, invopr");
                gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, src2_target, src2);
                checkGLcall("GL_TEXTURE_ENV, src2_target, src2");
                gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, opr2_target, opr2);
                checkGLcall("GL_TEXTURE_ENV, opr2_target, opr2");
                gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, src3_target, GL_TEXTURE);
                checkGLcall("GL_TEXTURE_ENV, src3_target, GL_TEXTURE");
                gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, opr3_target, GL_ONE_MINUS_SRC_ALPHA);
                checkGLcall("GL_TEXTURE_ENV, opr3_target, GL_ONE_MINUS_SRC_ALPHA");
                gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, scal_target, 1);
                checkGLcall("GL_TEXTURE_ENV, scal_target, 1");
                break;
            case WINED3D_TOP_BLEND_FACTOR_ALPHA:
                gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, comb_target, GL_ADD);
                checkGLcall("GL_TEXTURE_ENV, comb_target, GL_ADD");
                gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, src0_target, src1);
                checkGLcall("GL_TEXTURE_ENV, src0_target, src1");
                gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, opr0_target, opr1);
                checkGLcall("GL_TEXTURE_ENV, opr0_target, opr1");
                gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, src1_target, GL_CONSTANT);
                checkGLcall("GL_TEXTURE_ENV, src1_target, GL_CONSTANT");
                gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, opr1_target, invopr);
                checkGLcall("GL_TEXTURE_ENV, opr1_target, invopr");
                gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, src2_target, src2);
                checkGLcall("GL_TEXTURE_ENV, src2_target, src2");
                gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, opr2_target, opr2);
                checkGLcall("GL_TEXTURE_ENV, opr2_target, opr2");
                gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, src3_target, GL_CONSTANT);
                checkGLcall("GL_TEXTURE_ENV, src3_target, GL_CONSTANT");
                gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, opr3_target, GL_ONE_MINUS_SRC_ALPHA);
                checkGLcall("GL_TEXTURE_ENV, opr3_target, GL_ONE_MINUS_SRC_ALPHA");
                gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, scal_target, 1);
                checkGLcall("GL_TEXTURE_ENV, scal_target, 1");
                break;
            case WINED3D_TOP_BLEND_TEXTURE_ALPHA_PM:
                gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, comb_target, GL_ADD);
                checkGLcall("GL_TEXTURE_ENV, comb_target, GL_ADD");
                gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, src0_target, src1);
                checkGLcall("GL_TEXTURE_ENV, src0_target, src1");
                gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, opr0_target, opr1);
                checkGLcall("GL_TEXTURE_ENV, opr0_target, opr1");
                gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, src1_target, GL_ZERO);
                checkGLcall("GL_TEXTURE_ENV, src1_target, GL_ZERO");
                gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, opr1_target, invopr);
                checkGLcall("GL_TEXTURE_ENV, opr1_target, invopr");
                gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, src2_target, src2);
                checkGLcall("GL_TEXTURE_ENV, src2_target, src2");
                gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, opr2_target, opr2);
                checkGLcall("GL_TEXTURE_ENV, opr2_target, opr2");
                gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, src3_target, GL_TEXTURE);
                checkGLcall("GL_TEXTURE_ENV, src3_target, GL_TEXTURE");
                gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, opr3_target, GL_ONE_MINUS_SRC_ALPHA);
                checkGLcall("GL_TEXTURE_ENV, opr3_target, GL_ONE_MINUS_SRC_ALPHA");
                gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, scal_target, 1);
                checkGLcall("GL_TEXTURE_ENV, scal_target, 1");
                break;
            case WINED3D_TOP_MODULATE_ALPHA_ADD_COLOR:
                gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, comb_target, GL_ADD);
                checkGLcall("GL_TEXTURE_ENV, comb_target, GL_ADD");                 /* Add = a0*a1 + a2*a3 */
                gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, src0_target, src1);  /*  a0 = src1/opr1     */
                checkGLcall("GL_TEXTURE_ENV, src0_target, src1");
                gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, opr0_target, opr1);
                checkGLcall("GL_TEXTURE_ENV, opr0_target, opr1");                   /*  a1 = 1 (see docs)  */
                gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, src1_target, GL_ZERO);
                checkGLcall("GL_TEXTURE_ENV, src1_target, GL_ZERO");
                gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, opr1_target, invopr);
                checkGLcall("GL_TEXTURE_ENV, opr1_target, invopr");
                gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, src2_target, src2);  /*  a2 = arg2          */
                checkGLcall("GL_TEXTURE_ENV, src2_target, src2");
                gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, opr2_target, opr2);
                checkGLcall("GL_TEXTURE_ENV, opr2_target, opr2");                   /*  a3 = src1 alpha    */
                gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, src3_target, src1);
                checkGLcall("GL_TEXTURE_ENV, src3_target, src1");
                switch (opr) {
                    case GL_SRC_COLOR: opr = GL_SRC_ALPHA; break;
                    case GL_ONE_MINUS_SRC_COLOR: opr = GL_ONE_MINUS_SRC_ALPHA; break;
                }
                gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, opr3_target, opr);
                checkGLcall("GL_TEXTURE_ENV, opr3_target, opr");
                gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, scal_target, 1);
                checkGLcall("GL_TEXTURE_ENV, scal_target, 1");
                break;
            case WINED3D_TOP_MODULATE_COLOR_ADD_ALPHA:
                gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, comb_target, GL_ADD);
                checkGLcall("GL_TEXTURE_ENV, comb_target, GL_ADD");
                gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, src0_target, src1);
                checkGLcall("GL_TEXTURE_ENV, src0_target, src1");
                gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, opr0_target, opr1);
                checkGLcall("GL_TEXTURE_ENV, opr0_target, opr1");
                gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, src1_target, src2);
                checkGLcall("GL_TEXTURE_ENV, src1_target, src2");
                gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, opr1_target, opr2);
                checkGLcall("GL_TEXTURE_ENV, opr1_target, opr2");
                gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, src2_target, src1);
                checkGLcall("GL_TEXTURE_ENV, src2_target, src1");
                switch (opr1) {
                    case GL_SRC_COLOR: opr = GL_SRC_ALPHA; break;
                    case GL_ONE_MINUS_SRC_COLOR: opr = GL_ONE_MINUS_SRC_ALPHA; break;
                }
                gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, opr2_target, opr);
                checkGLcall("GL_TEXTURE_ENV, opr2_target, opr");
                gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, src3_target, GL_ZERO);
                checkGLcall("GL_TEXTURE_ENV, src3_target, GL_ZERO");
                gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, opr3_target, invopr);
                checkGLcall("GL_TEXTURE_ENV, opr3_target, invopr");
                gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, scal_target, 1);
                checkGLcall("GL_TEXTURE_ENV, scal_target, 1");
                break;
            case WINED3D_TOP_MODULATE_INVALPHA_ADD_COLOR:
                gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, comb_target, GL_ADD);
                checkGLcall("GL_TEXTURE_ENV, comb_target, GL_ADD");
                gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, src0_target, src1);
                checkGLcall("GL_TEXTURE_ENV, src0_target, src1");
                gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, opr0_target, opr1);
                checkGLcall("GL_TEXTURE_ENV, opr0_target, opr1");
                gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, src1_target, GL_ZERO);
                checkGLcall("GL_TEXTURE_ENV, src1_target, GL_ZERO");
                gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, opr1_target, invopr);
                checkGLcall("GL_TEXTURE_ENV, opr1_target, invopr");
                gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, src2_target, src2);
                checkGLcall("GL_TEXTURE_ENV, src2_target, src2");
                gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, opr2_target, opr2);
                checkGLcall("GL_TEXTURE_ENV, opr2_target, opr2");
                gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, src3_target, src1);
                checkGLcall("GL_TEXTURE_ENV, src3_target, src1");
                switch (opr1) {
                    case GL_SRC_COLOR: opr = GL_ONE_MINUS_SRC_ALPHA; break;
                    case GL_ONE_MINUS_SRC_COLOR: opr = GL_SRC_ALPHA; break;
                    case GL_SRC_ALPHA: opr = GL_ONE_MINUS_SRC_ALPHA; break;
                    case GL_ONE_MINUS_SRC_ALPHA: opr = GL_SRC_ALPHA; break;
                }
                gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, opr3_target, opr);
                checkGLcall("GL_TEXTURE_ENV, opr3_target, opr");
                gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, scal_target, 1);
                checkGLcall("GL_TEXTURE_ENV, scal_target, 1");
                break;
            case WINED3D_TOP_MODULATE_INVCOLOR_ADD_ALPHA:
                gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, comb_target, GL_ADD);
                checkGLcall("GL_TEXTURE_ENV, comb_target, GL_ADD");
                gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, src0_target, src1);
                checkGLcall("GL_TEXTURE_ENV, src0_target, src1");
                switch (opr1) {
                    case GL_SRC_COLOR: opr = GL_ONE_MINUS_SRC_COLOR; break;
                    case GL_ONE_MINUS_SRC_COLOR: opr = GL_SRC_COLOR; break;
                    case GL_SRC_ALPHA: opr = GL_ONE_MINUS_SRC_ALPHA; break;
                    case GL_ONE_MINUS_SRC_ALPHA: opr = GL_SRC_ALPHA; break;
                }
                gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, opr0_target, opr);
                checkGLcall("GL_TEXTURE_ENV, opr0_target, opr");
                gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, src1_target, src2);
                checkGLcall("GL_TEXTURE_ENV, src1_target, src2");
                gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, opr1_target, opr2);
                checkGLcall("GL_TEXTURE_ENV, opr1_target, opr2");
                gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, src2_target, src1);
                checkGLcall("GL_TEXTURE_ENV, src2_target, src1");
                switch (opr1) {
                    case GL_SRC_COLOR: opr = GL_SRC_ALPHA; break;
                    case GL_ONE_MINUS_SRC_COLOR: opr = GL_ONE_MINUS_SRC_ALPHA; break;
                }
                gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, opr2_target, opr);
                checkGLcall("GL_TEXTURE_ENV, opr2_target, opr");
                gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, src3_target, GL_ZERO);
                checkGLcall("GL_TEXTURE_ENV, src3_target, GL_ZERO");
                gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, opr3_target, invopr);
                checkGLcall("GL_TEXTURE_ENV, opr3_target, invopr");
                gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, scal_target, 1);
                checkGLcall("GL_TEXTURE_ENV, scal_target, 1");
                break;
            case WINED3D_TOP_MULTIPLY_ADD:
                gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, comb_target, GL_ADD);
                checkGLcall("GL_TEXTURE_ENV, comb_target, GL_ADD");
                gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, src0_target, src3);
                checkGLcall("GL_TEXTURE_ENV, src0_target, src1");
                gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, opr0_target, opr3);
                checkGLcall("GL_TEXTURE_ENV, opr0_target, opr1");
                gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, src1_target, GL_ZERO);
                checkGLcall("GL_TEXTURE_ENV, src1_target, GL_ZERO");
                gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, opr1_target, invopr);
                checkGLcall("GL_TEXTURE_ENV, opr1_target, invopr");
                gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, src2_target, src1);
                checkGLcall("GL_TEXTURE_ENV, src2_target, src2");
                gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, opr2_target, opr1);
                checkGLcall("GL_TEXTURE_ENV, opr2_target, opr2");
                gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, src3_target, src2);
                checkGLcall("GL_TEXTURE_ENV, src3_target, src3");
                gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, opr3_target, opr2);
                checkGLcall("GL_TEXTURE_ENV, opr3_target, opr3");
                gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, scal_target, 1);
                checkGLcall("GL_TEXTURE_ENV, scal_target, 1");
                break;

            case WINED3D_TOP_BUMPENVMAP:
            case WINED3D_TOP_BUMPENVMAP_LUMINANCE:
                FIXME("Implement bump environment mapping in GL_NV_texture_env_combine4 path\n");
                Handled = FALSE;
                break;

            default:
                Handled = FALSE;
        }
        if (Handled)
        {
            gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE4_NV);
            checkGLcall("GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE4_NV");

            return;
        }
    } /* GL_NV_texture_env_combine4 */

    Handled = TRUE; /* Again, assume handled */
    switch (op) {
        case WINED3D_TOP_DISABLE: /* Only for alpha */
            gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, comb_target, GL_REPLACE);
            checkGLcall("GL_TEXTURE_ENV, comb_target, GL_REPLACE");
            gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, src0_target, GL_PREVIOUS_EXT);
            checkGLcall("GL_TEXTURE_ENV, src0_target, GL_PREVIOUS_EXT");
            gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, opr0_target, GL_SRC_ALPHA);
            checkGLcall("GL_TEXTURE_ENV, opr0_target, GL_SRC_ALPHA");
            gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, scal_target, 1);
            checkGLcall("GL_TEXTURE_ENV, scal_target, 1");
            break;
        case WINED3D_TOP_SELECT_ARG1:
            gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, comb_target, GL_REPLACE);
            checkGLcall("GL_TEXTURE_ENV, comb_target, GL_REPLACE");
            gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, src0_target, src1);
            checkGLcall("GL_TEXTURE_ENV, src0_target, src1");
            gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, opr0_target, opr1);
            checkGLcall("GL_TEXTURE_ENV, opr0_target, opr1");
            gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, scal_target, 1);
            checkGLcall("GL_TEXTURE_ENV, scal_target, 1");
            break;
        case WINED3D_TOP_SELECT_ARG2:
            gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, comb_target, GL_REPLACE);
            checkGLcall("GL_TEXTURE_ENV, comb_target, GL_REPLACE");
            gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, src0_target, src2);
            checkGLcall("GL_TEXTURE_ENV, src0_target, src2");
            gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, opr0_target, opr2);
            checkGLcall("GL_TEXTURE_ENV, opr0_target, opr2");
            gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, scal_target, 1);
            checkGLcall("GL_TEXTURE_ENV, scal_target, 1");
            break;
        case WINED3D_TOP_MODULATE:
            gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, comb_target, GL_MODULATE);
            checkGLcall("GL_TEXTURE_ENV, comb_target, GL_MODULATE");
            gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, src0_target, src1);
            checkGLcall("GL_TEXTURE_ENV, src0_target, src1");
            gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, opr0_target, opr1);
            checkGLcall("GL_TEXTURE_ENV, opr0_target, opr1");
            gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, src1_target, src2);
            checkGLcall("GL_TEXTURE_ENV, src1_target, src2");
            gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, opr1_target, opr2);
            checkGLcall("GL_TEXTURE_ENV, opr1_target, opr2");
            gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, scal_target, 1);
            checkGLcall("GL_TEXTURE_ENV, scal_target, 1");
            break;
        case WINED3D_TOP_MODULATE_2X:
            gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, comb_target, GL_MODULATE);
            checkGLcall("GL_TEXTURE_ENV, comb_target, GL_MODULATE");
            gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, src0_target, src1);
            checkGLcall("GL_TEXTURE_ENV, src0_target, src1");
            gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, opr0_target, opr1);
            checkGLcall("GL_TEXTURE_ENV, opr0_target, opr1");
            gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, src1_target, src2);
            checkGLcall("GL_TEXTURE_ENV, src1_target, src2");
            gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, opr1_target, opr2);
            checkGLcall("GL_TEXTURE_ENV, opr1_target, opr2");
            gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, scal_target, 2);
            checkGLcall("GL_TEXTURE_ENV, scal_target, 2");
            break;
        case WINED3D_TOP_MODULATE_4X:
            gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, comb_target, GL_MODULATE);
            checkGLcall("GL_TEXTURE_ENV, comb_target, GL_MODULATE");
            gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, src0_target, src1);
            checkGLcall("GL_TEXTURE_ENV, src0_target, src1");
            gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, opr0_target, opr1);
            checkGLcall("GL_TEXTURE_ENV, opr0_target, opr1");
            gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, src1_target, src2);
            checkGLcall("GL_TEXTURE_ENV, src1_target, src2");
            gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, opr1_target, opr2);
            checkGLcall("GL_TEXTURE_ENV, opr1_target, opr2");
            gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, scal_target, 4);
            checkGLcall("GL_TEXTURE_ENV, scal_target, 4");
            break;
        case WINED3D_TOP_ADD:
            gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, comb_target, GL_ADD);
            checkGLcall("GL_TEXTURE_ENV, comb_target, GL_ADD");
            gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, src0_target, src1);
            checkGLcall("GL_TEXTURE_ENV, src0_target, src1");
            gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, opr0_target, opr1);
            checkGLcall("GL_TEXTURE_ENV, opr0_target, opr1");
            gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, src1_target, src2);
            checkGLcall("GL_TEXTURE_ENV, src1_target, src2");
            gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, opr1_target, opr2);
            checkGLcall("GL_TEXTURE_ENV, opr1_target, opr2");
            gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, scal_target, 1);
            checkGLcall("GL_TEXTURE_ENV, scal_target, 1");
            break;
        case WINED3D_TOP_ADD_SIGNED:
            gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, comb_target, GL_ADD_SIGNED);
            checkGLcall("GL_TEXTURE_ENV, comb_target, GL_ADD_SIGNED");
            gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, src0_target, src1);
            checkGLcall("GL_TEXTURE_ENV, src0_target, src1");
            gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, opr0_target, opr1);
            checkGLcall("GL_TEXTURE_ENV, opr0_target, opr1");
            gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, src1_target, src2);
            checkGLcall("GL_TEXTURE_ENV, src1_target, src2");
            gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, opr1_target, opr2);
            checkGLcall("GL_TEXTURE_ENV, opr1_target, opr2");
            gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, scal_target, 1);
            checkGLcall("GL_TEXTURE_ENV, scal_target, 1");
            break;
        case WINED3D_TOP_ADD_SIGNED_2X:
            gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, comb_target, GL_ADD_SIGNED);
            checkGLcall("GL_TEXTURE_ENV, comb_target, GL_ADD_SIGNED");
            gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, src0_target, src1);
            checkGLcall("GL_TEXTURE_ENV, src0_target, src1");
            gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, opr0_target, opr1);
            checkGLcall("GL_TEXTURE_ENV, opr0_target, opr1");
            gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, src1_target, src2);
            checkGLcall("GL_TEXTURE_ENV, src1_target, src2");
            gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, opr1_target, opr2);
            checkGLcall("GL_TEXTURE_ENV, opr1_target, opr2");
            gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, scal_target, 2);
            checkGLcall("GL_TEXTURE_ENV, scal_target, 2");
            break;
        case WINED3D_TOP_SUBTRACT:
            if (gl_info->supported[ARB_TEXTURE_ENV_COMBINE])
            {
                gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, comb_target, GL_SUBTRACT);
                checkGLcall("GL_TEXTURE_ENV, comb_target, GL_SUBTRACT");
                gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, src0_target, src1);
                checkGLcall("GL_TEXTURE_ENV, src0_target, src1");
                gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, opr0_target, opr1);
                checkGLcall("GL_TEXTURE_ENV, opr0_target, opr1");
                gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, src1_target, src2);
                checkGLcall("GL_TEXTURE_ENV, src1_target, src2");
                gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, opr1_target, opr2);
                checkGLcall("GL_TEXTURE_ENV, opr1_target, opr2");
                gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, scal_target, 1);
                checkGLcall("GL_TEXTURE_ENV, scal_target, 1");
            } else {
                FIXME("This version of opengl does not support GL_SUBTRACT\n");
            }
            break;

        case WINED3D_TOP_BLEND_DIFFUSE_ALPHA:
            gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, comb_target, GL_INTERPOLATE);
            checkGLcall("GL_TEXTURE_ENV, comb_target, GL_INTERPOLATE");
            gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, src0_target, src1);
            checkGLcall("GL_TEXTURE_ENV, src0_target, src1");
            gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, opr0_target, opr1);
            checkGLcall("GL_TEXTURE_ENV, opr0_target, opr1");
            gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, src1_target, src2);
            checkGLcall("GL_TEXTURE_ENV, src1_target, src2");
            gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, opr1_target, opr2);
            checkGLcall("GL_TEXTURE_ENV, opr1_target, opr2");
            gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, src2_target, GL_PRIMARY_COLOR);
            checkGLcall("GL_TEXTURE_ENV, src2_target, GL_PRIMARY_COLOR");
            gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, opr2_target, GL_SRC_ALPHA);
            checkGLcall("GL_TEXTURE_ENV, opr2_target, GL_SRC_ALPHA");
            gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, scal_target, 1);
            checkGLcall("GL_TEXTURE_ENV, scal_target, 1");
            break;
        case WINED3D_TOP_BLEND_TEXTURE_ALPHA:
            gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, comb_target, GL_INTERPOLATE);
            checkGLcall("GL_TEXTURE_ENV, comb_target, GL_INTERPOLATE");
            gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, src0_target, src1);
            checkGLcall("GL_TEXTURE_ENV, src0_target, src1");
            gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, opr0_target, opr1);
            checkGLcall("GL_TEXTURE_ENV, opr0_target, opr1");
            gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, src1_target, src2);
            checkGLcall("GL_TEXTURE_ENV, src1_target, src2");
            gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, opr1_target, opr2);
            checkGLcall("GL_TEXTURE_ENV, opr1_target, opr2");
            gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, src2_target, GL_TEXTURE);
            checkGLcall("GL_TEXTURE_ENV, src2_target, GL_TEXTURE");
            gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, opr2_target, GL_SRC_ALPHA);
            checkGLcall("GL_TEXTURE_ENV, opr2_target, GL_SRC_ALPHA");
            gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, scal_target, 1);
            checkGLcall("GL_TEXTURE_ENV, scal_target, 1");
            break;
        case WINED3D_TOP_BLEND_FACTOR_ALPHA:
            gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, comb_target, GL_INTERPOLATE);
            checkGLcall("GL_TEXTURE_ENV, comb_target, GL_INTERPOLATE");
            gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, src0_target, src1);
            checkGLcall("GL_TEXTURE_ENV, src0_target, src1");
            gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, opr0_target, opr1);
            checkGLcall("GL_TEXTURE_ENV, opr0_target, opr1");
            gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, src1_target, src2);
            checkGLcall("GL_TEXTURE_ENV, src1_target, src2");
            gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, opr1_target, opr2);
            checkGLcall("GL_TEXTURE_ENV, opr1_target, opr2");
            gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, src2_target, GL_CONSTANT);
            checkGLcall("GL_TEXTURE_ENV, src2_target, GL_CONSTANT");
            gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, opr2_target, GL_SRC_ALPHA);
            checkGLcall("GL_TEXTURE_ENV, opr2_target, GL_SRC_ALPHA");
            gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, scal_target, 1);
            checkGLcall("GL_TEXTURE_ENV, scal_target, 1");
            break;
        case WINED3D_TOP_BLEND_CURRENT_ALPHA:
            gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, comb_target, GL_INTERPOLATE);
            checkGLcall("GL_TEXTURE_ENV, comb_target, GL_INTERPOLATE");
            gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, src0_target, src1);
            checkGLcall("GL_TEXTURE_ENV, src0_target, src1");
            gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, opr0_target, opr1);
            checkGLcall("GL_TEXTURE_ENV, opr0_target, opr1");
            gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, src1_target, src2);
            checkGLcall("GL_TEXTURE_ENV, src1_target, src2");
            gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, opr1_target, opr2);
            checkGLcall("GL_TEXTURE_ENV, opr1_target, opr2");
            gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, src2_target, GL_PREVIOUS);
            checkGLcall("GL_TEXTURE_ENV, src2_target, GL_PREVIOUS");
            gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, opr2_target, GL_SRC_ALPHA);
            checkGLcall("GL_TEXTURE_ENV, opr2_target, GL_SRC_ALPHA");
            gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, scal_target, 1);
            checkGLcall("GL_TEXTURE_ENV, scal_target, 1");
            break;
        case WINED3D_TOP_DOTPRODUCT3:
            if (gl_info->supported[ARB_TEXTURE_ENV_DOT3])
            {
                gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, comb_target, GL_DOT3_RGBA_ARB);
                checkGLcall("GL_TEXTURE_ENV, comb_target, GL_DOT3_RGBA_ARB");
            }
            else if (gl_info->supported[EXT_TEXTURE_ENV_DOT3])
            {
                gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, comb_target, GL_DOT3_RGBA_EXT);
                checkGLcall("GL_TEXTURE_ENV, comb_target, GL_DOT3_RGBA_EXT");
            } else {
                FIXME("This version of opengl does not support GL_DOT3\n");
            }
            gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, src0_target, src1);
            checkGLcall("GL_TEXTURE_ENV, src0_target, src1");
            gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, opr0_target, opr1);
            checkGLcall("GL_TEXTURE_ENV, opr0_target, opr1");
            gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, src1_target, src2);
            checkGLcall("GL_TEXTURE_ENV, src1_target, src2");
            gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, opr1_target, opr2);
            checkGLcall("GL_TEXTURE_ENV, opr1_target, opr2");
            gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, scal_target, 1);
            checkGLcall("GL_TEXTURE_ENV, scal_target, 1");
            break;
        case WINED3D_TOP_LERP:
            gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, comb_target, GL_INTERPOLATE);
            checkGLcall("GL_TEXTURE_ENV, comb_target, GL_INTERPOLATE");
            gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, src0_target, src1);
            checkGLcall("GL_TEXTURE_ENV, src0_target, src1");
            gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, opr0_target, opr1);
            checkGLcall("GL_TEXTURE_ENV, opr0_target, opr1");
            gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, src1_target, src2);
            checkGLcall("GL_TEXTURE_ENV, src1_target, src2");
            gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, opr1_target, opr2);
            checkGLcall("GL_TEXTURE_ENV, opr1_target, opr2");
            gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, src2_target, src3);
            checkGLcall("GL_TEXTURE_ENV, src2_target, src3");
            gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, opr2_target, opr3);
            checkGLcall("GL_TEXTURE_ENV, opr2_target, opr3");
            gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, scal_target, 1);
            checkGLcall("GL_TEXTURE_ENV, scal_target, 1");
            break;
        case WINED3D_TOP_ADD_SMOOTH:
            if (gl_info->supported[ATI_TEXTURE_ENV_COMBINE3])
            {
                gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, comb_target, GL_MODULATE_ADD_ATI);
                checkGLcall("GL_TEXTURE_ENV, comb_target, GL_MODULATE_ADD_ATI");
                gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, src0_target, src1);
                checkGLcall("GL_TEXTURE_ENV, src0_target, src1");
                switch (opr1) {
                    case GL_SRC_COLOR: opr = GL_ONE_MINUS_SRC_COLOR; break;
                    case GL_ONE_MINUS_SRC_COLOR: opr = GL_SRC_COLOR; break;
                    case GL_SRC_ALPHA: opr = GL_ONE_MINUS_SRC_ALPHA; break;
                    case GL_ONE_MINUS_SRC_ALPHA: opr = GL_SRC_ALPHA; break;
                }
                gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, opr0_target, opr);
                checkGLcall("GL_TEXTURE_ENV, opr0_target, opr");
                gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, src1_target, src1);
                checkGLcall("GL_TEXTURE_ENV, src1_target, src1");
                gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, opr1_target, opr1);
                checkGLcall("GL_TEXTURE_ENV, opr1_target, opr1");
                gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, src2_target, src2);
                checkGLcall("GL_TEXTURE_ENV, src2_target, src2");
                gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, opr2_target, opr2);
                checkGLcall("GL_TEXTURE_ENV, opr2_target, opr2");
                gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, scal_target, 1);
                checkGLcall("GL_TEXTURE_ENV, scal_target, 1");
            } else
                Handled = FALSE;
            break;
        case WINED3D_TOP_BLEND_TEXTURE_ALPHA_PM:
            if (gl_info->supported[ATI_TEXTURE_ENV_COMBINE3])
            {
                gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, comb_target, GL_MODULATE_ADD_ATI);
                checkGLcall("GL_TEXTURE_ENV, comb_target, GL_MODULATE_ADD_ATI");
                gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, src0_target, GL_TEXTURE);
                checkGLcall("GL_TEXTURE_ENV, src0_target, GL_TEXTURE");
                gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, opr0_target, GL_ONE_MINUS_SRC_ALPHA);
                checkGLcall("GL_TEXTURE_ENV, opr0_target, GL_ONE_MINUS_SRC_APHA");
                gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, src1_target, src1);
                checkGLcall("GL_TEXTURE_ENV, src1_target, src1");
                gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, opr1_target, opr1);
                checkGLcall("GL_TEXTURE_ENV, opr1_target, opr1");
                gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, src2_target, src2);
                checkGLcall("GL_TEXTURE_ENV, src2_target, src2");
                gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, opr2_target, opr2);
                checkGLcall("GL_TEXTURE_ENV, opr2_target, opr2");
                gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, scal_target, 1);
                checkGLcall("GL_TEXTURE_ENV, scal_target, 1");
            } else
                Handled = FALSE;
            break;
        case WINED3D_TOP_MODULATE_ALPHA_ADD_COLOR:
            if (gl_info->supported[ATI_TEXTURE_ENV_COMBINE3])
            {
                gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, comb_target, GL_MODULATE_ADD_ATI);
                checkGLcall("GL_TEXTURE_ENV, comb_target, GL_MODULATE_ADD_ATI");
                gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, src0_target, src1);
                checkGLcall("GL_TEXTURE_ENV, src0_target, src1");
                switch (opr1) {
                    case GL_SRC_COLOR: opr = GL_SRC_ALPHA; break;
                    case GL_ONE_MINUS_SRC_COLOR: opr = GL_ONE_MINUS_SRC_ALPHA; break;
                    case GL_SRC_ALPHA: opr = GL_SRC_ALPHA; break;
                    case GL_ONE_MINUS_SRC_ALPHA: opr = GL_ONE_MINUS_SRC_ALPHA; break;
                }
                gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, opr0_target, opr);
                checkGLcall("GL_TEXTURE_ENV, opr0_target, opr");
                gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, src1_target, src1);
                checkGLcall("GL_TEXTURE_ENV, src1_target, src1");
                gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, opr1_target, opr1);
                checkGLcall("GL_TEXTURE_ENV, opr1_target, opr1");
                gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, src2_target, src2);
                checkGLcall("GL_TEXTURE_ENV, src2_target, src2");
                gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, opr2_target, opr2);
                checkGLcall("GL_TEXTURE_ENV, opr2_target, opr2");
                gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, scal_target, 1);
                checkGLcall("GL_TEXTURE_ENV, scal_target, 1");
            } else
                Handled = FALSE;
            break;
        case WINED3D_TOP_MODULATE_COLOR_ADD_ALPHA:
            if (gl_info->supported[ATI_TEXTURE_ENV_COMBINE3])
            {
                gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, comb_target, GL_MODULATE_ADD_ATI);
                checkGLcall("GL_TEXTURE_ENV, comb_target, GL_MODULATE_ADD_ATI");
                gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, src0_target, src1);
                checkGLcall("GL_TEXTURE_ENV, src0_target, src1");
                gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, opr0_target, opr1);
                checkGLcall("GL_TEXTURE_ENV, opr0_target, opr1");
                gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, src1_target, src1);
                checkGLcall("GL_TEXTURE_ENV, src1_target, src1");
                switch (opr1) {
                    case GL_SRC_COLOR: opr = GL_SRC_ALPHA; break;
                    case GL_ONE_MINUS_SRC_COLOR: opr = GL_ONE_MINUS_SRC_ALPHA; break;
                    case GL_SRC_ALPHA: opr = GL_SRC_ALPHA; break;
                    case GL_ONE_MINUS_SRC_ALPHA: opr = GL_ONE_MINUS_SRC_ALPHA; break;
                }
                gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, opr1_target, opr);
                checkGLcall("GL_TEXTURE_ENV, opr1_target, opr");
                gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, src2_target, src2);
                checkGLcall("GL_TEXTURE_ENV, src2_target, src2");
                gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, opr2_target, opr2);
                checkGLcall("GL_TEXTURE_ENV, opr2_target, opr2");
                gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, scal_target, 1);
                checkGLcall("GL_TEXTURE_ENV, scal_target, 1");
            } else
                Handled = FALSE;
            break;
        case WINED3D_TOP_MODULATE_INVALPHA_ADD_COLOR:
            if (gl_info->supported[ATI_TEXTURE_ENV_COMBINE3])
            {
                gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, comb_target, GL_MODULATE_ADD_ATI);
                checkGLcall("GL_TEXTURE_ENV, comb_target, GL_MODULATE_ADD_ATI");
                gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, src0_target, src1);
                checkGLcall("GL_TEXTURE_ENV, src0_target, src1");
                switch (opr1) {
                    case GL_SRC_COLOR: opr = GL_ONE_MINUS_SRC_ALPHA; break;
                    case GL_ONE_MINUS_SRC_COLOR: opr = GL_SRC_ALPHA; break;
                    case GL_SRC_ALPHA: opr = GL_ONE_MINUS_SRC_ALPHA; break;
                    case GL_ONE_MINUS_SRC_ALPHA: opr = GL_SRC_ALPHA; break;
                }
                gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, opr0_target, opr);
                checkGLcall("GL_TEXTURE_ENV, opr0_target, opr");
                gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, src1_target, src1);
                checkGLcall("GL_TEXTURE_ENV, src1_target, src1");
                gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, opr1_target, opr1);
                checkGLcall("GL_TEXTURE_ENV, opr1_target, opr1");
                gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, src2_target, src2);
                checkGLcall("GL_TEXTURE_ENV, src2_target, src2");
                gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, opr2_target, opr2);
                checkGLcall("GL_TEXTURE_ENV, opr2_target, opr2");
                gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, scal_target, 1);
                checkGLcall("GL_TEXTURE_ENV, scal_target, 1");
            } else
                Handled = FALSE;
            break;
        case WINED3D_TOP_MODULATE_INVCOLOR_ADD_ALPHA:
            if (gl_info->supported[ATI_TEXTURE_ENV_COMBINE3])
            {
                gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, comb_target, GL_MODULATE_ADD_ATI);
                checkGLcall("GL_TEXTURE_ENV, comb_target, GL_MODULATE_ADD_ATI");
                gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, src0_target, src1);
                checkGLcall("GL_TEXTURE_ENV, src0_target, src1");
                switch (opr1) {
                    case GL_SRC_COLOR: opr = GL_ONE_MINUS_SRC_COLOR; break;
                    case GL_ONE_MINUS_SRC_COLOR: opr = GL_SRC_COLOR; break;
                    case GL_SRC_ALPHA: opr = GL_ONE_MINUS_SRC_ALPHA; break;
                    case GL_ONE_MINUS_SRC_ALPHA: opr = GL_SRC_ALPHA; break;
                }
                gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, opr0_target, opr);
                checkGLcall("GL_TEXTURE_ENV, opr0_target, opr");
                gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, src1_target, src1);
                checkGLcall("GL_TEXTURE_ENV, src1_target, src1");
                switch (opr1) {
                    case GL_SRC_COLOR: opr = GL_SRC_ALPHA; break;
                    case GL_ONE_MINUS_SRC_COLOR: opr = GL_ONE_MINUS_SRC_ALPHA; break;
                    case GL_SRC_ALPHA: opr = GL_SRC_ALPHA; break;
                    case GL_ONE_MINUS_SRC_ALPHA: opr = GL_ONE_MINUS_SRC_ALPHA; break;
                }
                gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, opr1_target, opr);
                checkGLcall("GL_TEXTURE_ENV, opr1_target, opr");
                gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, src2_target, src2);
                checkGLcall("GL_TEXTURE_ENV, src2_target, src2");
                gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, opr2_target, opr2);
                checkGLcall("GL_TEXTURE_ENV, opr2_target, opr2");
                gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, scal_target, 1);
                checkGLcall("GL_TEXTURE_ENV, scal_target, 1");
            } else
                Handled = FALSE;
            break;
        case WINED3D_TOP_MULTIPLY_ADD:
            if (gl_info->supported[ATI_TEXTURE_ENV_COMBINE3])
            {
                gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, comb_target, GL_MODULATE_ADD_ATI);
                checkGLcall("GL_TEXTURE_ENV, comb_target, GL_MODULATE_ADD_ATI");
                gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, src0_target, src1);
                checkGLcall("GL_TEXTURE_ENV, src0_target, src1");
                gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, opr0_target, opr1);
                checkGLcall("GL_TEXTURE_ENV, opr0_target, opr1");
                gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, src1_target, src3);
                checkGLcall("GL_TEXTURE_ENV, src1_target, src3");
                gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, opr1_target, opr3);
                checkGLcall("GL_TEXTURE_ENV, opr1_target, opr3");
                gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, src2_target, src2);
                checkGLcall("GL_TEXTURE_ENV, src2_target, src2");
                gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, opr2_target, opr2);
                checkGLcall("GL_TEXTURE_ENV, opr2_target, opr2");
                gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, scal_target, 1);
                checkGLcall("GL_TEXTURE_ENV, scal_target, 1");
            } else
                Handled = FALSE;
            break;
        case WINED3D_TOP_BUMPENVMAP_LUMINANCE:
        case WINED3D_TOP_BUMPENVMAP:
            if (gl_info->supported[NV_TEXTURE_SHADER2])
            {
                /* Technically texture shader support without register combiners is possible, but not expected to occur
                 * on real world cards, so for now a fixme should be enough
                 */
                FIXME("Implement bump mapping with GL_NV_texture_shader in non register combiner path\n");
            }
            Handled = FALSE;
            break;

        default:
            Handled = FALSE;
    }

    if (Handled) {
        BOOL  combineOK = TRUE;
        if (gl_info->supported[NV_TEXTURE_ENV_COMBINE4])
        {
            DWORD op2;

            if (isAlpha)
                op2 = state->texture_states[Stage][WINED3D_TSS_COLOR_OP];
            else
                op2 = state->texture_states[Stage][WINED3D_TSS_ALPHA_OP];

            /* Note: If COMBINE4 in effect can't go back to combine! */
            switch (op2)
            {
                case WINED3D_TOP_ADD_SMOOTH:
                case WINED3D_TOP_BLEND_TEXTURE_ALPHA_PM:
                case WINED3D_TOP_MODULATE_ALPHA_ADD_COLOR:
                case WINED3D_TOP_MODULATE_COLOR_ADD_ALPHA:
                case WINED3D_TOP_MODULATE_INVALPHA_ADD_COLOR:
                case WINED3D_TOP_MODULATE_INVCOLOR_ADD_ALPHA:
                case WINED3D_TOP_MULTIPLY_ADD:
                    /* Ignore those implemented in both cases */
                    switch (op)
                    {
                        case WINED3D_TOP_SELECT_ARG1:
                        case WINED3D_TOP_SELECT_ARG2:
                            combineOK = FALSE;
                            Handled   = FALSE;
                            break;
                        default:
                            FIXME("Can't use COMBINE4 and COMBINE together, thisop=%s, otherop=%s, isAlpha(%d)\n", debug_d3dtop(op), debug_d3dtop(op2), isAlpha);
                            return;
                    }
            }
        }

        if (combineOK)
        {
            gl_info->gl_ops.gl.p_glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE);
            checkGLcall("GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE");

            return;
        }
    }

    /* After all the extensions, if still unhandled, report fixme */
    FIXME("Unhandled texture operation %s\n", debug_d3dtop(op));
}


static void tex_colorop(struct wined3d_context *context, const struct wined3d_state *state, DWORD state_id)
{
    unsigned int stage = (state_id - STATE_TEXTURESTAGE(0, 0)) / (WINED3D_HIGHEST_TEXTURE_STATE + 1);
    struct wined3d_context_gl *context_gl = wined3d_context_gl(context);
    BOOL tex_used = context->fixed_function_usage_map & (1u << stage);
    unsigned int mapped_stage = context_gl->tex_unit_map[stage];
    const struct wined3d_gl_info *gl_info = context_gl->gl_info;

    TRACE("Setting color op for stage %d\n", stage);

    /* Using a pixel shader? Don't care for anything here, the shader applying does it */
    if (use_ps(state)) return;

    if (stage != mapped_stage) WARN("Using non 1:1 mapping: %d -> %d!\n", stage, mapped_stage);

    if (mapped_stage != WINED3D_UNMAPPED_STAGE)
    {
        if (tex_used && mapped_stage >= gl_info->limits.ffp_textures)
        {
            FIXME("Attempt to enable unsupported stage!\n");
            return;
        }
        wined3d_context_gl_active_texture(context_gl, gl_info, mapped_stage);
    }

    if (stage >= context->lowest_disabled_stage)
    {
        TRACE("Stage disabled\n");
        if (mapped_stage != WINED3D_UNMAPPED_STAGE)
        {
            /* Disable everything here */
            gl_info->gl_ops.gl.p_glDisable(GL_TEXTURE_2D);
            checkGLcall("glDisable(GL_TEXTURE_2D)");
            gl_info->gl_ops.gl.p_glDisable(GL_TEXTURE_3D);
            checkGLcall("glDisable(GL_TEXTURE_3D)");
            if (gl_info->supported[ARB_TEXTURE_CUBE_MAP])
            {
                gl_info->gl_ops.gl.p_glDisable(GL_TEXTURE_CUBE_MAP_ARB);
                checkGLcall("glDisable(GL_TEXTURE_CUBE_MAP_ARB)");
            }
            if (gl_info->supported[ARB_TEXTURE_RECTANGLE])
            {
                gl_info->gl_ops.gl.p_glDisable(GL_TEXTURE_RECTANGLE_ARB);
                checkGLcall("glDisable(GL_TEXTURE_RECTANGLE_ARB)");
            }
        }
        /* All done */
        return;
    }

    /* The sampler will also activate the correct texture dimensions, so no
     * need to do it here if the sampler for this stage is dirty. */
    if (!isStateDirty(context, STATE_SAMPLER(stage)) && tex_used)
        texture_activate_dimensions(wined3d_state_get_ffp_texture(state, stage), gl_info);

    set_tex_op(gl_info, state, FALSE, stage,
            state->texture_states[stage][WINED3D_TSS_COLOR_OP],
            state->texture_states[stage][WINED3D_TSS_COLOR_ARG1],
            state->texture_states[stage][WINED3D_TSS_COLOR_ARG2],
            state->texture_states[stage][WINED3D_TSS_COLOR_ARG0]);
}

void tex_alphaop(struct wined3d_context *context, const struct wined3d_state *state, DWORD state_id)
{
    unsigned int stage = (state_id - STATE_TEXTURESTAGE(0, 0)) / (WINED3D_HIGHEST_TEXTURE_STATE + 1);
    struct wined3d_texture *texture = wined3d_state_get_ffp_texture(state, 0);
    struct wined3d_context_gl *context_gl = wined3d_context_gl(context);
    BOOL tex_used = context->fixed_function_usage_map & (1u << stage);
    const struct wined3d_gl_info *gl_info = context_gl->gl_info;
    unsigned int mapped_stage = context_gl->tex_unit_map[stage];
    DWORD op, arg1, arg2, arg0;

    TRACE("Setting alpha op for stage %d\n", stage);
    /* Do not care for enabled / disabled stages, just assign the settings. colorop disables / enables required stuff */
    if (mapped_stage != WINED3D_UNMAPPED_STAGE)
    {
        if (tex_used && mapped_stage >= gl_info->limits.ffp_textures)
        {
            FIXME("Attempt to enable unsupported stage!\n");
            return;
        }
        wined3d_context_gl_active_texture(context_gl, gl_info, mapped_stage);
    }

    op = state->texture_states[stage][WINED3D_TSS_ALPHA_OP];
    arg1 = state->texture_states[stage][WINED3D_TSS_ALPHA_ARG1];
    arg2 = state->texture_states[stage][WINED3D_TSS_ALPHA_ARG2];
    arg0 = state->texture_states[stage][WINED3D_TSS_ALPHA_ARG0];

    if (state->render_states[WINED3D_RS_COLORKEYENABLE] && !stage && texture)
    {
        struct wined3d_texture_gl *texture_gl = wined3d_texture_gl(texture);
        GLenum texture_dimensions = texture_gl->target;

        if (texture_dimensions == GL_TEXTURE_2D || texture_dimensions == GL_TEXTURE_RECTANGLE_ARB)
        {
            if (texture_gl->t.async.color_key_flags & WINED3D_CKEY_SRC_BLT
                    && !texture_gl->t.resource.format->alpha_size)
            {
                /* Color keying needs to pass alpha values from the texture through to have the alpha test work
                 * properly. On the other hand applications can still use texture combiners apparently. This code
                 * takes care that apps cannot remove the texture's alpha channel entirely.
                 *
                 * The fixup is required for Prince of Persia 3D(prison bars), while Moto racer 2 requires
                 * D3DTOP_MODULATE to work on color keyed surfaces. Aliens vs Predator 1 uses color keyed textures
                 * and alpha component of diffuse color to draw things like translucent text and perform other
                 * blending effects.
                 *
                 * Aliens vs Predator 1 relies on diffuse alpha having an effect, so it cannot be ignored. To
                 * provide the behavior expected by the game, while emulating the colorkey, diffuse alpha must be
                 * modulated with texture alpha. OTOH, Moto racer 2 at some points sets alphaop/alphaarg to
                 * SELECTARG/CURRENT, yet puts garbage in diffuse alpha (zeroes). This works on native, because the
                 * game disables alpha test and alpha blending. Alpha test is overwritten by wine's for purposes of
                 * color-keying though, so this will lead to missing geometry if texture alpha is modulated (pixels
                 * fail alpha test). To get around this, blend state is checked: if the app enables alpha blending,
                 * it can be expected to provide meaningful values in diffuse alpha, so it should be modulated with
                 * texture alpha; otherwise, selecting diffuse alpha is ignored in favour of texture alpha.
                 *
                 * What to do with multitexturing? So far no app has been found that uses color keying with
                 * multitexturing */
                if (op == WINED3D_TOP_DISABLE)
                {
                    arg1 = WINED3DTA_TEXTURE;
                    op = WINED3D_TOP_SELECT_ARG1;
                }
                else if (op == WINED3D_TOP_SELECT_ARG1 && arg1 != WINED3DTA_TEXTURE)
                {
                    if (state->blend_state && state->blend_state->desc.rt[0].enable)
                    {
                        arg2 = WINED3DTA_TEXTURE;
                        op = WINED3D_TOP_MODULATE;
                    }
                    else arg1 = WINED3DTA_TEXTURE;
                }
                else if (op == WINED3D_TOP_SELECT_ARG2 && arg2 != WINED3DTA_TEXTURE)
                {
                    if (state->blend_state && state->blend_state->desc.rt[0].enable)
                    {
                        arg1 = WINED3DTA_TEXTURE;
                        op = WINED3D_TOP_MODULATE;
                    }
                    else arg2 = WINED3DTA_TEXTURE;
                }
            }
        }
    }

    /* tex_alphaop is shared between the ffp and nvrc because the difference only comes down to
     * this if block here, and the other code(color keying, texture unit selection) are the same
     */
    TRACE("Setting alpha op for stage %d\n", stage);
    if (gl_info->supported[NV_REGISTER_COMBINERS])
    {
        set_tex_op_nvrc(gl_info, state, TRUE, stage, op, arg1, arg2, arg0,
                mapped_stage, state->texture_states[stage][WINED3D_TSS_RESULT_ARG]);
    }
    else
    {
        set_tex_op(gl_info, state, TRUE, stage, op, arg1, arg2, arg0);
    }
}

static void sampler(struct wined3d_context *context, const struct wined3d_state *state, DWORD state_id)
{
}

void apply_pixelshader(struct wined3d_context *context, const struct wined3d_state *state, DWORD state_id)
{
    unsigned int i;

    if (use_ps(state))
    {
        if (!context->last_was_pshader)
        {
            /* Former draw without a pixel shader, some samplers may be
             * disabled because of WINED3D_TSS_COLOR_OP = WINED3DTOP_DISABLE
             * make sure to enable them. */
            context->update_shader_resource_bindings = 1;
            context->last_was_pshader = TRUE;
        }
        else
        {
            /* Otherwise all samplers were activated by the code above in
             * earlier draws, or by sampler() if a different texture was
             * bound. I don't have to do anything. */
        }
    }
    else
    {
        /* Disabled the pixel shader - color ops weren't applied while it was
         * enabled, so re-apply them. */
        for (i = 0; i < context->d3d_info->ffp_fragment_caps.max_blend_stages; ++i)
        {
            if (!isStateDirty(context, STATE_TEXTURESTAGE(i, WINED3D_TSS_COLOR_OP)))
                context_apply_state(context, state, STATE_TEXTURESTAGE(i, WINED3D_TSS_COLOR_OP));
        }
        context->last_was_pshader = FALSE;
    }

    context->shader_update_mask |= 1u << WINED3D_SHADER_TYPE_PIXEL;
}

static void state_compute_shader(struct wined3d_context *context, const struct wined3d_state *state, DWORD state_id)
{
    context->shader_update_mask |= 1u << WINED3D_SHADER_TYPE_COMPUTE;
}

static void state_shader(struct wined3d_context *context, const struct wined3d_state *state, DWORD state_id)
{
    enum wined3d_shader_type shader_type = state_id - STATE_SHADER(0);
    context->shader_update_mask |= 1u << shader_type;
}

static void shader_bumpenv(struct wined3d_context *context, const struct wined3d_state *state, DWORD state_id)
{
    context->constant_update_mask |= WINED3D_SHADER_CONST_PS_BUMP_ENV;
}

void clipplane(struct wined3d_context *context, const struct wined3d_state *state, DWORD state_id)
{
    const struct wined3d_gl_info *gl_info = wined3d_context_gl(context)->gl_info;
    UINT index = state_id - STATE_CLIPPLANE(0);
    GLdouble plane[4];

    if (index >= gl_info->limits.user_clip_distances)
        return;

    gl_info->gl_ops.gl.p_glMatrixMode(GL_MODELVIEW);
    gl_info->gl_ops.gl.p_glPushMatrix();

    /* Clip Plane settings are affected by the model view in OpenGL, the View transform in direct3d */
    if (!use_vs(state))
        gl_info->gl_ops.gl.p_glLoadMatrixf(&state->transforms[WINED3D_TS_VIEW]._11);
    else
        /* With vertex shaders, clip planes are not transformed in Direct3D,
         * while in OpenGL they are still transformed by the model view matrix. */
        gl_info->gl_ops.gl.p_glLoadIdentity();

    plane[0] = state->clip_planes[index].x;
    plane[1] = state->clip_planes[index].y;
    plane[2] = state->clip_planes[index].z;
    plane[3] = state->clip_planes[index].w;

    TRACE("Clipplane [%.8e, %.8e, %.8e, %.8e]\n",
            plane[0], plane[1], plane[2], plane[3]);
    gl_info->gl_ops.gl.p_glClipPlane(GL_CLIP_PLANE0 + index, plane);
    checkGLcall("glClipPlane");

    gl_info->gl_ops.gl.p_glPopMatrix();
}

static void streamsrc(struct wined3d_context *context, const struct wined3d_state *state, DWORD state_id)
{
    wined3d_context_gl_update_stream_sources(wined3d_context_gl(context), state);
}

static void vdecl_miscpart(struct wined3d_context *context, const struct wined3d_state *state, DWORD state_id)
{
    if (isStateDirty(context, STATE_STREAMSRC))
        return;
    wined3d_context_gl_update_stream_sources(wined3d_context_gl(context), state);
}

static void viewport_miscpart(struct wined3d_context *context, const struct wined3d_state *state, DWORD state_id)
{
    const struct wined3d_gl_info *gl_info = wined3d_context_gl(context)->gl_info;
    float min_z, max_z;

    if (gl_info->supported[ARB_VIEWPORT_ARRAY])
    {
        GLdouble depth_ranges[2 * WINED3D_MAX_VIEWPORTS];
        GLfloat viewports[4 * WINED3D_MAX_VIEWPORTS];

        unsigned int i, reset_count = 0;

        for (i = 0; i < state->viewport_count; ++i)
        {
            wined3d_viewport_get_z_range(&state->viewports[i], &min_z, &max_z);
            depth_ranges[i * 2] = min_z;
            depth_ranges[i * 2 + 1] = max_z;

            viewports[i * 4]     = state->viewports[i].x;
            viewports[i * 4 + 1] = state->viewports[i].y;
            viewports[i * 4 + 2] = state->viewports[i].width;
            viewports[i * 4 + 3] = state->viewports[i].height;

            /* Don't pass fractionals to GL if we earlier decided not to use
             * this functionality for two reasons: First, GL might offer us
             * fewer than 8 bits, and still make use of the fractional, in
             * addition to the emulation we apply in shader_get_position_fixup.
             * Second, even if GL tells us it has no subpixel precision (Mac OS!)
             * it might still do something with the fractional amount, e.g.
             * round it upwards. I can't find any info on rounding in
             * GL_ARB_viewport_array. */
            if (!context->d3d_info->subpixel_viewport)
            {
                viewports[i * 4]     = floor(viewports[i * 4]);
                viewports[i * 4 + 1] = floor(viewports[i * 4 + 1]);
                viewports[i * 4 + 2] = floor(viewports[i * 4 + 2]);
                viewports[i * 4 + 3] = floor(viewports[i * 4 + 3]);
            }
        }

        if (context->viewport_count > state->viewport_count)
            reset_count = context->viewport_count - state->viewport_count;

        if (reset_count)
        {
            memset(&depth_ranges[state->viewport_count * 2], 0, reset_count * 2 * sizeof(*depth_ranges));
            memset(&viewports[state->viewport_count * 4], 0, reset_count * 4 * sizeof(*viewports));
        }

        GL_EXTCALL(glDepthRangeArrayv(0, state->viewport_count + reset_count, depth_ranges));
        GL_EXTCALL(glViewportArrayv(0, state->viewport_count + reset_count, viewports));
        context->viewport_count = state->viewport_count;
    }
    else
    {
        wined3d_viewport_get_z_range(&state->viewports[0], &min_z, &max_z);
        gl_info->gl_ops.gl.p_glDepthRange(min_z, max_z);
        gl_info->gl_ops.gl.p_glViewport(state->viewports[0].x, state->viewports[0].y,
                state->viewports[0].width, state->viewports[0].height);
    }
    checkGLcall("setting clip space and viewport");
}

static void viewport_miscpart_cc(struct wined3d_context *context,
        const struct wined3d_state *state, DWORD state_id)
{
    const struct wined3d_gl_info *gl_info = wined3d_context_gl(context)->gl_info;
    /* See get_projection_matrix() in utils.c for a discussion about those values. */
    float pixel_center_offset = context->d3d_info->wined3d_creation_flags
            & WINED3D_PIXEL_CENTER_INTEGER ? 0.5f : 0.0f;
    GLdouble depth_ranges[2 * WINED3D_MAX_VIEWPORTS];
    GLfloat viewports[4 * WINED3D_MAX_VIEWPORTS];
    unsigned int i, reset_count = 0;
    float min_z, max_z;

    pixel_center_offset += context->d3d_info->filling_convention_offset / 2.0f;

    GL_EXTCALL(glClipControl(GL_UPPER_LEFT, GL_ZERO_TO_ONE));

    for (i = 0; i < state->viewport_count; ++i)
    {
        wined3d_viewport_get_z_range(&state->viewports[i], &min_z, &max_z);
        depth_ranges[i * 2] = min_z;
        depth_ranges[i * 2 + 1] = max_z;

        viewports[i * 4] = state->viewports[i].x + pixel_center_offset;
        viewports[i * 4 + 1] = state->viewports[i].y + pixel_center_offset;
        viewports[i * 4 + 2] = state->viewports[i].width;
        viewports[i * 4 + 3] = state->viewports[i].height;
    }

    if (context->viewport_count > state->viewport_count)
        reset_count = context->viewport_count - state->viewport_count;

    if (reset_count)
    {
        memset(&depth_ranges[state->viewport_count * 2], 0, reset_count * 2 * sizeof(*depth_ranges));
        memset(&viewports[state->viewport_count * 4], 0, reset_count * 4 * sizeof(*viewports));
    }

    GL_EXTCALL(glDepthRangeArrayv(0, state->viewport_count + reset_count, depth_ranges));
    GL_EXTCALL(glViewportArrayv(0, state->viewport_count + reset_count, viewports));
    context->viewport_count = state->viewport_count;

    checkGLcall("setting clip space and viewport");
}

static void scissorrect(struct wined3d_context *context, const struct wined3d_state *state, DWORD state_id)
{
    const struct wined3d_gl_info *gl_info = wined3d_context_gl(context)->gl_info;
    const RECT *r;

    /* Warning: glScissor uses window coordinates, not viewport coordinates,
     * so our viewport correction does not apply. Warning2: Even in windowed
     * mode the coords are relative to the window, not the screen. */

    if (gl_info->supported[ARB_VIEWPORT_ARRAY])
    {
        GLint sr[4 * WINED3D_MAX_VIEWPORTS];
        unsigned int i, reset_count = 0;

        for (i = 0; i < state->scissor_rect_count; ++i)
        {
            r = &state->scissor_rects[i];

            sr[i * 4] = r->left;
            sr[i * 4 + 1] = r->top;
            sr[i * 4 + 2] = r->right - r->left;
            sr[i * 4 + 3] = r->bottom - r->top;
        }

        if (context->scissor_rect_count > state->scissor_rect_count)
            reset_count = context->scissor_rect_count - state->scissor_rect_count;

        if (reset_count)
            memset(&sr[state->scissor_rect_count * 4], 0, reset_count * 4 * sizeof(GLint));

        GL_EXTCALL(glScissorArrayv(0, state->scissor_rect_count + reset_count, sr));
        checkGLcall("glScissorArrayv");
        context->scissor_rect_count = state->scissor_rect_count;
    }
    else
    {
        r = &state->scissor_rects[0];
        gl_info->gl_ops.gl.p_glScissor(r->left, r->top, r->right - r->left, r->bottom - r->top);
        checkGLcall("glScissor");
    }
}

static void indexbuffer(struct wined3d_context *context, const struct wined3d_state *state, DWORD state_id)
{
    const struct wined3d_gl_info *gl_info = wined3d_context_gl(context)->gl_info;
    const struct wined3d_stream_info *stream_info = &context->stream_info;
    struct wined3d_buffer *buffer;

    if (!state->index_buffer || !stream_info->all_vbo)
    {
        GL_EXTCALL(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0));
        return;
    }

    buffer = state->index_buffer;
    if (buffer->buffer_object)
    {
        GL_EXTCALL(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, wined3d_bo_gl(buffer->buffer_object)->id));
        wined3d_buffer_validate_user(buffer);
    }
    else
    {
        GL_EXTCALL(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0));
    }
}

static void depth_clip(const struct wined3d_rasterizer_state *r, const struct wined3d_gl_info *gl_info)
{
    if (!gl_info->supported[ARB_DEPTH_CLAMP])
    {
        if (r && !r->desc.depth_clip)
            FIXME("Depth clamp not supported by this GL implementation.\n");
        return;
    }

    if (r && !r->desc.depth_clip)
        gl_info->gl_ops.gl.p_glEnable(GL_DEPTH_CLAMP);
    else
        gl_info->gl_ops.gl.p_glDisable(GL_DEPTH_CLAMP);
    checkGLcall("depth clip");
}

static void rasterizer(struct wined3d_context *context, const struct wined3d_state *state, DWORD state_id)
{
    const struct wined3d_gl_info *gl_info = wined3d_context_gl(context)->gl_info;
    const struct wined3d_rasterizer_state *r = state->rasterizer_state;

    /* Rendering without ARB_clip_control requires flipping position manually.
     * This also means that all primitives will be backwards, so we need to
     * also swap which side is the front face. */
    gl_info->gl_ops.gl.p_glFrontFace(r && r->desc.front_ccw ? GL_CW : GL_CCW);
    checkGLcall("glFrontFace");
    depthbias(context, state);
    fillmode(r, gl_info);
    cullmode(r, gl_info);
    depth_clip(r, gl_info);
    scissor(r, gl_info);
    line_antialias(r, gl_info);
}

static void rasterizer_cc(struct wined3d_context *context, const struct wined3d_state *state, DWORD state_id)
{
    const struct wined3d_gl_info *gl_info = wined3d_context_gl(context)->gl_info;
    const struct wined3d_rasterizer_state *r = state->rasterizer_state;
    GLenum mode;

    mode = r && r->desc.front_ccw ? GL_CCW : GL_CW;

    gl_info->gl_ops.gl.p_glFrontFace(mode);
    checkGLcall("glFrontFace");
    depthbias(context, state);
    fillmode(r, gl_info);
    cullmode(r, gl_info);
    depth_clip(r, gl_info);
    scissor(r, gl_info);
    line_antialias(r, gl_info);
}

static void psorigin_w(struct wined3d_context *context, const struct wined3d_state *state, DWORD state_id)
{
    static BOOL warned;

    if (!warned)
    {
        WARN("Point sprite coordinate origin switching not supported.\n");
        warned = TRUE;
    }
}

static void psorigin(struct wined3d_context *context, const struct wined3d_state *state, DWORD state_id)
{
    const struct wined3d_gl_info *gl_info = wined3d_context_gl(context)->gl_info;

    GL_EXTCALL(glPointParameteri(GL_POINT_SPRITE_COORD_ORIGIN, GL_LOWER_LEFT));
    checkGLcall("glPointParameteri(GL_POINT_SPRITE_COORD_ORIGIN, ...)");
}

void state_srgbwrite(struct wined3d_context *context, const struct wined3d_state *state, DWORD state_id)
{
    const struct wined3d_gl_info *gl_info = wined3d_context_gl(context)->gl_info;

    TRACE("context %p, state %p, state_id %#lx.\n", context, state, state_id);

    if (needs_srgb_write(context->d3d_info, state, &state->fb))
        gl_info->gl_ops.gl.p_glEnable(GL_FRAMEBUFFER_SRGB);
    else
        gl_info->gl_ops.gl.p_glDisable(GL_FRAMEBUFFER_SRGB);
}

static void state_cb(struct wined3d_context *context, const struct wined3d_state *state, DWORD state_id)
{
    const struct wined3d_gl_info *gl_info = wined3d_context_gl(context)->gl_info;
    enum wined3d_shader_type shader_type;
    struct wined3d_buffer *buffer;
    unsigned int i, base, count;
    struct wined3d_bo_gl *bo_gl;

    TRACE("context %p, state %p, state_id %#lx.\n", context, state, state_id);

    if (STATE_IS_GRAPHICS_CONSTANT_BUFFER(state_id))
        shader_type = state_id - STATE_GRAPHICS_CONSTANT_BUFFER(0);
    else
        shader_type = WINED3D_SHADER_TYPE_COMPUTE;

    /* If a shader has not been set, buffer objects are not yet initialised. */
    if (!state->shader[shader_type])
        return;

    wined3d_gl_limits_get_uniform_block_range(&gl_info->limits, shader_type, &base, &count);
    for (i = 0; i < count; ++i)
    {
        const struct wined3d_constant_buffer_state *buffer_state = &state->cb[shader_type][i];

        if (!buffer_state->buffer)
        {
            GL_EXTCALL(glBindBufferBase(GL_UNIFORM_BUFFER, base + i, 0));
            continue;
        }

        buffer = buffer_state->buffer;
        bo_gl = wined3d_bo_gl(buffer->buffer_object);
        GL_EXTCALL(glBindBufferRange(GL_UNIFORM_BUFFER, base + i,
                bo_gl->id, bo_gl->b.buffer_offset + buffer_state->offset,
                min(buffer_state->size, buffer->resource.size - buffer_state->offset)));

        wined3d_buffer_validate_user(buffer);
    }
    checkGLcall("bind constant buffers");
}

static void state_cb_warn(struct wined3d_context *context, const struct wined3d_state *state, DWORD state_id)
{
    TRACE("context %p, state %p, state_id %#lx.\n", context, state, state_id);

    WARN("Constant buffers (%s) no supported.\n", debug_d3dstate(state_id));
}

static void state_shader_resource_binding(struct wined3d_context *context,
        const struct wined3d_state *state, DWORD state_id)
{
    TRACE("context %p, state %p, state_id %#lx.\n", context, state, state_id);

    context->update_shader_resource_bindings = 1;
}

static void state_cs_resource_binding(struct wined3d_context *context,
        const struct wined3d_state *state, DWORD state_id)
{
    TRACE("context %p, state %p, state_id %#lx.\n", context, state, state_id);
    context->update_compute_shader_resource_bindings = 1;
}

static void state_uav_binding(struct wined3d_context *context,
        const struct wined3d_state *state, DWORD state_id)
{
    TRACE("context %p, state %p, state_id %#lx.\n", context, state, state_id);
    context->update_unordered_access_view_bindings = 1;
}

static void state_cs_uav_binding(struct wined3d_context *context,
        const struct wined3d_state *state, DWORD state_id)
{
    TRACE("context %p, state %p, state_id %#lx.\n", context, state, state_id);
    context->update_compute_unordered_access_view_bindings = 1;
}

static void state_uav_warn(struct wined3d_context *context, const struct wined3d_state *state, DWORD state_id)
{
    WARN("ARB_shader_image_load_store is not supported by this OpenGL implementation.\n");
}

static void state_so(struct wined3d_context *context, const struct wined3d_state *state, DWORD state_id)
{
    struct wined3d_context_gl *context_gl = wined3d_context_gl(context);
    const struct wined3d_gl_info *gl_info = context_gl->gl_info;
    struct wined3d_buffer *buffer;
    unsigned int offset, size, i;
    struct wined3d_bo_gl *bo_gl;

    TRACE("context %p, state %p, state_id %#lx.\n", context, state, state_id);

    wined3d_context_gl_end_transform_feedback(context_gl);

    for (i = 0; i < ARRAY_SIZE(state->stream_output); ++i)
    {
        if (!state->stream_output[i].buffer)
        {
            GL_EXTCALL(glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, i, 0));
            continue;
        }

        buffer = state->stream_output[i].buffer;
        offset = state->stream_output[i].offset;
        bo_gl = wined3d_bo_gl(buffer->buffer_object);
        if (offset == ~0u)
        {
            FIXME("Appending to stream output buffers not implemented.\n");
            offset = 0;
        }
        size = buffer->resource.size - offset;
        GL_EXTCALL(glBindBufferRange(GL_TRANSFORM_FEEDBACK_BUFFER, i,
                bo_gl->id, bo_gl->b.buffer_offset + offset, size));
        wined3d_buffer_validate_user(buffer);
    }
    checkGLcall("bind transform feedback buffers");
}

static void state_so_warn(struct wined3d_context *context, const struct wined3d_state *state, DWORD state_id)
{
    WARN("Transform feedback not supported.\n");
}

const struct wined3d_state_entry_template misc_state_template_gl[] =
{
    { STATE_CONSTANT_BUFFER(WINED3D_SHADER_TYPE_VERTEX),  { STATE_CONSTANT_BUFFER(WINED3D_SHADER_TYPE_VERTEX),  state_cb,           }, ARB_UNIFORM_BUFFER_OBJECT       },
    { STATE_CONSTANT_BUFFER(WINED3D_SHADER_TYPE_VERTEX),  { STATE_CONSTANT_BUFFER(WINED3D_SHADER_TYPE_VERTEX),  state_cb_warn,      }, WINED3D_GL_EXT_NONE             },
    { STATE_CONSTANT_BUFFER(WINED3D_SHADER_TYPE_HULL),    { STATE_CONSTANT_BUFFER(WINED3D_SHADER_TYPE_HULL),    state_cb,           }, ARB_UNIFORM_BUFFER_OBJECT       },
    { STATE_CONSTANT_BUFFER(WINED3D_SHADER_TYPE_HULL),    { STATE_CONSTANT_BUFFER(WINED3D_SHADER_TYPE_HULL),    state_cb_warn,      }, WINED3D_GL_EXT_NONE             },
    { STATE_CONSTANT_BUFFER(WINED3D_SHADER_TYPE_DOMAIN),  { STATE_CONSTANT_BUFFER(WINED3D_SHADER_TYPE_DOMAIN),  state_cb,           }, ARB_UNIFORM_BUFFER_OBJECT       },
    { STATE_CONSTANT_BUFFER(WINED3D_SHADER_TYPE_DOMAIN),  { STATE_CONSTANT_BUFFER(WINED3D_SHADER_TYPE_DOMAIN),  state_cb_warn,      }, WINED3D_GL_EXT_NONE             },
    { STATE_CONSTANT_BUFFER(WINED3D_SHADER_TYPE_GEOMETRY),{ STATE_CONSTANT_BUFFER(WINED3D_SHADER_TYPE_GEOMETRY),state_cb,           }, ARB_UNIFORM_BUFFER_OBJECT       },
    { STATE_CONSTANT_BUFFER(WINED3D_SHADER_TYPE_GEOMETRY),{ STATE_CONSTANT_BUFFER(WINED3D_SHADER_TYPE_GEOMETRY),state_cb_warn,      }, WINED3D_GL_EXT_NONE             },
    { STATE_CONSTANT_BUFFER(WINED3D_SHADER_TYPE_PIXEL),   { STATE_CONSTANT_BUFFER(WINED3D_SHADER_TYPE_PIXEL),   state_cb,           }, ARB_UNIFORM_BUFFER_OBJECT       },
    { STATE_CONSTANT_BUFFER(WINED3D_SHADER_TYPE_PIXEL),   { STATE_CONSTANT_BUFFER(WINED3D_SHADER_TYPE_PIXEL),   state_cb_warn,      }, WINED3D_GL_EXT_NONE             },
    { STATE_CONSTANT_BUFFER(WINED3D_SHADER_TYPE_COMPUTE), { STATE_CONSTANT_BUFFER(WINED3D_SHADER_TYPE_COMPUTE), state_cb,           }, ARB_UNIFORM_BUFFER_OBJECT       },
    { STATE_CONSTANT_BUFFER(WINED3D_SHADER_TYPE_COMPUTE), { STATE_CONSTANT_BUFFER(WINED3D_SHADER_TYPE_COMPUTE), state_cb_warn,      }, WINED3D_GL_EXT_NONE             },
    { STATE_GRAPHICS_SHADER_RESOURCE_BINDING,             { STATE_GRAPHICS_SHADER_RESOURCE_BINDING,             state_shader_resource_binding}, WINED3D_GL_EXT_NONE    },
    { STATE_GRAPHICS_UNORDERED_ACCESS_VIEW_BINDING,       { STATE_GRAPHICS_UNORDERED_ACCESS_VIEW_BINDING,       state_uav_binding   }, ARB_SHADER_IMAGE_LOAD_STORE     },
    { STATE_GRAPHICS_UNORDERED_ACCESS_VIEW_BINDING,       { STATE_GRAPHICS_UNORDERED_ACCESS_VIEW_BINDING,       state_uav_warn      }, WINED3D_GL_EXT_NONE             },
    { STATE_COMPUTE_SHADER_RESOURCE_BINDING,              { STATE_COMPUTE_SHADER_RESOURCE_BINDING,              state_cs_resource_binding}, WINED3D_GL_EXT_NONE        },
    { STATE_COMPUTE_UNORDERED_ACCESS_VIEW_BINDING,        { STATE_COMPUTE_UNORDERED_ACCESS_VIEW_BINDING,        state_cs_uav_binding}, ARB_SHADER_IMAGE_LOAD_STORE     },
    { STATE_COMPUTE_UNORDERED_ACCESS_VIEW_BINDING,        { STATE_COMPUTE_UNORDERED_ACCESS_VIEW_BINDING,        state_uav_warn      }, WINED3D_GL_EXT_NONE             },
    { STATE_STREAM_OUTPUT,                                { STATE_STREAM_OUTPUT,                                state_so,           }, WINED3D_GL_VERSION_3_2          },
    { STATE_STREAM_OUTPUT,                                { STATE_STREAM_OUTPUT,                                state_so_warn,      }, WINED3D_GL_EXT_NONE             },
    { STATE_BLEND,                                        { STATE_BLEND,                                        blend_dbb           }, ARB_DRAW_BUFFERS_BLEND          },
    { STATE_BLEND,                                        { STATE_BLEND,                                        blend_db2           }, EXT_DRAW_BUFFERS2               },
    { STATE_BLEND,                                        { STATE_BLEND,                                        blend               }, WINED3D_GL_EXT_NONE             },
    { STATE_BLEND_FACTOR,                                 { STATE_BLEND_FACTOR,                                 state_blend_factor  }, EXT_BLEND_COLOR                 },
    { STATE_BLEND_FACTOR,                                 { STATE_BLEND_FACTOR,                                 state_blend_factor_w}, WINED3D_GL_EXT_NONE             },
    { STATE_SAMPLE_MASK,                                  { STATE_SAMPLE_MASK,                                  state_sample_mask   }, ARB_TEXTURE_MULTISAMPLE         },
    { STATE_SAMPLE_MASK,                                  { STATE_SAMPLE_MASK,                                  state_sample_mask_w }, WINED3D_GL_EXT_NONE             },
    { STATE_DEPTH_STENCIL,                                { STATE_DEPTH_STENCIL,                                depth_stencil_2s    }, EXT_STENCIL_TWO_SIDE            },
    { STATE_DEPTH_STENCIL,                                { STATE_DEPTH_STENCIL,                                depth_stencil       }, WINED3D_GL_EXT_NONE             },
    { STATE_STENCIL_REF,                                  { STATE_DEPTH_STENCIL,                                NULL                }, WINED3D_GL_EXT_NONE             },
    { STATE_DEPTH_BOUNDS,                                 { STATE_DEPTH_STENCIL,                                NULL                }, WINED3D_GL_EXT_NONE             },
    { STATE_STREAMSRC,                                    { STATE_STREAMSRC,                                    streamsrc           }, WINED3D_GL_EXT_NONE             },
    { STATE_VDECL,                                        { STATE_VDECL,                                        vdecl_miscpart      }, WINED3D_GL_EXT_NONE             },
    { STATE_RASTERIZER,                                   { STATE_RASTERIZER,                                   rasterizer_cc       }, ARB_CLIP_CONTROL                },
    { STATE_RASTERIZER,                                   { STATE_RASTERIZER,                                   rasterizer          }, WINED3D_GL_EXT_NONE             },
    { STATE_SCISSORRECT,                                  { STATE_SCISSORRECT,                                  scissorrect         }, WINED3D_GL_EXT_NONE             },
    { STATE_POINTSPRITECOORDORIGIN,                       { STATE_POINTSPRITECOORDORIGIN,                       state_nop           }, ARB_CLIP_CONTROL                },
    { STATE_POINTSPRITECOORDORIGIN,                       { STATE_POINTSPRITECOORDORIGIN,                       psorigin            }, WINED3D_GL_VERSION_2_0          },
    { STATE_POINTSPRITECOORDORIGIN,                       { STATE_POINTSPRITECOORDORIGIN,                       psorigin_w          }, WINED3D_GL_EXT_NONE             },
    { STATE_TEXTURESTAGE(0, WINED3D_TSS_BUMPENV_MAT00),   { STATE_TEXTURESTAGE(0, WINED3D_TSS_BUMPENV_MAT00),   shader_bumpenv      }, WINED3D_GL_EXT_NONE             },
    { STATE_TEXTURESTAGE(0, WINED3D_TSS_BUMPENV_MAT01),   { STATE_TEXTURESTAGE(0, WINED3D_TSS_BUMPENV_MAT00),   NULL                }, WINED3D_GL_EXT_NONE             },
    { STATE_TEXTURESTAGE(0, WINED3D_TSS_BUMPENV_MAT10),   { STATE_TEXTURESTAGE(0, WINED3D_TSS_BUMPENV_MAT00),   NULL                }, WINED3D_GL_EXT_NONE             },
    { STATE_TEXTURESTAGE(0, WINED3D_TSS_BUMPENV_MAT11),   { STATE_TEXTURESTAGE(0, WINED3D_TSS_BUMPENV_MAT00),   NULL                }, WINED3D_GL_EXT_NONE             },
    { STATE_TEXTURESTAGE(1, WINED3D_TSS_BUMPENV_MAT00),   { STATE_TEXTURESTAGE(1, WINED3D_TSS_BUMPENV_MAT00),   shader_bumpenv      }, WINED3D_GL_EXT_NONE             },
    { STATE_TEXTURESTAGE(1, WINED3D_TSS_BUMPENV_MAT01),   { STATE_TEXTURESTAGE(1, WINED3D_TSS_BUMPENV_MAT00),   NULL                }, WINED3D_GL_EXT_NONE             },
    { STATE_TEXTURESTAGE(1, WINED3D_TSS_BUMPENV_MAT10),   { STATE_TEXTURESTAGE(1, WINED3D_TSS_BUMPENV_MAT00),   NULL                }, WINED3D_GL_EXT_NONE             },
    { STATE_TEXTURESTAGE(1, WINED3D_TSS_BUMPENV_MAT11),   { STATE_TEXTURESTAGE(1, WINED3D_TSS_BUMPENV_MAT00),   NULL                }, WINED3D_GL_EXT_NONE             },
    { STATE_TEXTURESTAGE(2, WINED3D_TSS_BUMPENV_MAT00),   { STATE_TEXTURESTAGE(2, WINED3D_TSS_BUMPENV_MAT00),   shader_bumpenv      }, WINED3D_GL_EXT_NONE             },
    { STATE_TEXTURESTAGE(2, WINED3D_TSS_BUMPENV_MAT01),   { STATE_TEXTURESTAGE(2, WINED3D_TSS_BUMPENV_MAT00),   NULL                }, WINED3D_GL_EXT_NONE             },
    { STATE_TEXTURESTAGE(2, WINED3D_TSS_BUMPENV_MAT10),   { STATE_TEXTURESTAGE(2, WINED3D_TSS_BUMPENV_MAT00),   NULL                }, WINED3D_GL_EXT_NONE             },
    { STATE_TEXTURESTAGE(2, WINED3D_TSS_BUMPENV_MAT11),   { STATE_TEXTURESTAGE(2, WINED3D_TSS_BUMPENV_MAT00),   NULL                }, WINED3D_GL_EXT_NONE             },
    { STATE_TEXTURESTAGE(3, WINED3D_TSS_BUMPENV_MAT00),   { STATE_TEXTURESTAGE(3, WINED3D_TSS_BUMPENV_MAT00),   shader_bumpenv      }, WINED3D_GL_EXT_NONE             },
    { STATE_TEXTURESTAGE(3, WINED3D_TSS_BUMPENV_MAT01),   { STATE_TEXTURESTAGE(3, WINED3D_TSS_BUMPENV_MAT00),   NULL                }, WINED3D_GL_EXT_NONE             },
    { STATE_TEXTURESTAGE(3, WINED3D_TSS_BUMPENV_MAT10),   { STATE_TEXTURESTAGE(3, WINED3D_TSS_BUMPENV_MAT00),   NULL                }, WINED3D_GL_EXT_NONE             },
    { STATE_TEXTURESTAGE(3, WINED3D_TSS_BUMPENV_MAT11),   { STATE_TEXTURESTAGE(3, WINED3D_TSS_BUMPENV_MAT00),   NULL                }, WINED3D_GL_EXT_NONE             },
    { STATE_TEXTURESTAGE(4, WINED3D_TSS_BUMPENV_MAT00),   { STATE_TEXTURESTAGE(4, WINED3D_TSS_BUMPENV_MAT00),   shader_bumpenv      }, WINED3D_GL_EXT_NONE             },
    { STATE_TEXTURESTAGE(4, WINED3D_TSS_BUMPENV_MAT01),   { STATE_TEXTURESTAGE(4, WINED3D_TSS_BUMPENV_MAT00),   NULL                }, WINED3D_GL_EXT_NONE             },
    { STATE_TEXTURESTAGE(4, WINED3D_TSS_BUMPENV_MAT10),   { STATE_TEXTURESTAGE(4, WINED3D_TSS_BUMPENV_MAT00),   NULL                }, WINED3D_GL_EXT_NONE             },
    { STATE_TEXTURESTAGE(4, WINED3D_TSS_BUMPENV_MAT11),   { STATE_TEXTURESTAGE(4, WINED3D_TSS_BUMPENV_MAT00),   NULL                }, WINED3D_GL_EXT_NONE             },
    { STATE_TEXTURESTAGE(5, WINED3D_TSS_BUMPENV_MAT00),   { STATE_TEXTURESTAGE(5, WINED3D_TSS_BUMPENV_MAT00),   shader_bumpenv      }, WINED3D_GL_EXT_NONE             },
    { STATE_TEXTURESTAGE(5, WINED3D_TSS_BUMPENV_MAT01),   { STATE_TEXTURESTAGE(5, WINED3D_TSS_BUMPENV_MAT00),   NULL                }, WINED3D_GL_EXT_NONE             },
    { STATE_TEXTURESTAGE(5, WINED3D_TSS_BUMPENV_MAT10),   { STATE_TEXTURESTAGE(5, WINED3D_TSS_BUMPENV_MAT00),   NULL                }, WINED3D_GL_EXT_NONE             },
    { STATE_TEXTURESTAGE(5, WINED3D_TSS_BUMPENV_MAT11),   { STATE_TEXTURESTAGE(5, WINED3D_TSS_BUMPENV_MAT00),   NULL                }, WINED3D_GL_EXT_NONE             },
    { STATE_TEXTURESTAGE(6, WINED3D_TSS_BUMPENV_MAT00),   { STATE_TEXTURESTAGE(6, WINED3D_TSS_BUMPENV_MAT00),   shader_bumpenv      }, WINED3D_GL_EXT_NONE             },
    { STATE_TEXTURESTAGE(6, WINED3D_TSS_BUMPENV_MAT01),   { STATE_TEXTURESTAGE(6, WINED3D_TSS_BUMPENV_MAT00),   NULL                }, WINED3D_GL_EXT_NONE             },
    { STATE_TEXTURESTAGE(6, WINED3D_TSS_BUMPENV_MAT10),   { STATE_TEXTURESTAGE(6, WINED3D_TSS_BUMPENV_MAT00),   NULL                }, WINED3D_GL_EXT_NONE             },
    { STATE_TEXTURESTAGE(6, WINED3D_TSS_BUMPENV_MAT11),   { STATE_TEXTURESTAGE(6, WINED3D_TSS_BUMPENV_MAT00),   NULL                }, WINED3D_GL_EXT_NONE             },
    { STATE_TEXTURESTAGE(7, WINED3D_TSS_BUMPENV_MAT00),   { STATE_TEXTURESTAGE(7, WINED3D_TSS_BUMPENV_MAT00),   shader_bumpenv      }, WINED3D_GL_EXT_NONE             },
    { STATE_TEXTURESTAGE(7, WINED3D_TSS_BUMPENV_MAT01),   { STATE_TEXTURESTAGE(7, WINED3D_TSS_BUMPENV_MAT00),   NULL                }, WINED3D_GL_EXT_NONE             },
    { STATE_TEXTURESTAGE(7, WINED3D_TSS_BUMPENV_MAT10),   { STATE_TEXTURESTAGE(7, WINED3D_TSS_BUMPENV_MAT00),   NULL                }, WINED3D_GL_EXT_NONE             },
    { STATE_TEXTURESTAGE(7, WINED3D_TSS_BUMPENV_MAT11),   { STATE_TEXTURESTAGE(7, WINED3D_TSS_BUMPENV_MAT00),   NULL                }, WINED3D_GL_EXT_NONE             },
    { STATE_TEXTURESTAGE(0, WINED3D_TSS_BUMPENV_LSCALE),  { STATE_TEXTURESTAGE(0, WINED3D_TSS_BUMPENV_LSCALE),  shader_bumpenv      }, WINED3D_GL_EXT_NONE             },
    { STATE_TEXTURESTAGE(0, WINED3D_TSS_BUMPENV_LOFFSET), { STATE_TEXTURESTAGE(0, WINED3D_TSS_BUMPENV_LSCALE),  NULL                }, WINED3D_GL_EXT_NONE             },
    { STATE_TEXTURESTAGE(1, WINED3D_TSS_BUMPENV_LSCALE),  { STATE_TEXTURESTAGE(1, WINED3D_TSS_BUMPENV_LSCALE),  shader_bumpenv      }, WINED3D_GL_EXT_NONE             },
    { STATE_TEXTURESTAGE(1, WINED3D_TSS_BUMPENV_LOFFSET), { STATE_TEXTURESTAGE(1, WINED3D_TSS_BUMPENV_LSCALE),  NULL                }, WINED3D_GL_EXT_NONE             },
    { STATE_TEXTURESTAGE(2, WINED3D_TSS_BUMPENV_LSCALE),  { STATE_TEXTURESTAGE(2, WINED3D_TSS_BUMPENV_LSCALE),  shader_bumpenv      }, WINED3D_GL_EXT_NONE             },
    { STATE_TEXTURESTAGE(2, WINED3D_TSS_BUMPENV_LOFFSET), { STATE_TEXTURESTAGE(2, WINED3D_TSS_BUMPENV_LSCALE),  NULL                }, WINED3D_GL_EXT_NONE             },
    { STATE_TEXTURESTAGE(3, WINED3D_TSS_BUMPENV_LSCALE),  { STATE_TEXTURESTAGE(3, WINED3D_TSS_BUMPENV_LSCALE),  shader_bumpenv      }, WINED3D_GL_EXT_NONE             },
    { STATE_TEXTURESTAGE(3, WINED3D_TSS_BUMPENV_LOFFSET), { STATE_TEXTURESTAGE(3, WINED3D_TSS_BUMPENV_LSCALE),  NULL                }, WINED3D_GL_EXT_NONE             },
    { STATE_TEXTURESTAGE(4, WINED3D_TSS_BUMPENV_LSCALE),  { STATE_TEXTURESTAGE(4, WINED3D_TSS_BUMPENV_LSCALE),  shader_bumpenv      }, WINED3D_GL_EXT_NONE             },
    { STATE_TEXTURESTAGE(4, WINED3D_TSS_BUMPENV_LOFFSET), { STATE_TEXTURESTAGE(4, WINED3D_TSS_BUMPENV_LSCALE),  NULL                }, WINED3D_GL_EXT_NONE             },
    { STATE_TEXTURESTAGE(5, WINED3D_TSS_BUMPENV_LSCALE),  { STATE_TEXTURESTAGE(5, WINED3D_TSS_BUMPENV_LSCALE),  shader_bumpenv      }, WINED3D_GL_EXT_NONE             },
    { STATE_TEXTURESTAGE(5, WINED3D_TSS_BUMPENV_LOFFSET), { STATE_TEXTURESTAGE(5, WINED3D_TSS_BUMPENV_LSCALE),  NULL                }, WINED3D_GL_EXT_NONE             },
    { STATE_TEXTURESTAGE(6, WINED3D_TSS_BUMPENV_LSCALE),  { STATE_TEXTURESTAGE(6, WINED3D_TSS_BUMPENV_LSCALE),  shader_bumpenv      }, WINED3D_GL_EXT_NONE             },
    { STATE_TEXTURESTAGE(6, WINED3D_TSS_BUMPENV_LOFFSET), { STATE_TEXTURESTAGE(6, WINED3D_TSS_BUMPENV_LSCALE),  NULL                }, WINED3D_GL_EXT_NONE             },
    { STATE_TEXTURESTAGE(7, WINED3D_TSS_BUMPENV_LSCALE),  { STATE_TEXTURESTAGE(7, WINED3D_TSS_BUMPENV_LSCALE),  shader_bumpenv      }, WINED3D_GL_EXT_NONE             },
    { STATE_TEXTURESTAGE(7, WINED3D_TSS_BUMPENV_LOFFSET), { STATE_TEXTURESTAGE(7, WINED3D_TSS_BUMPENV_LSCALE),  NULL                }, WINED3D_GL_EXT_NONE             },

    { STATE_VIEWPORT,                                     { STATE_VIEWPORT,                                     viewport_miscpart_cc}, ARB_CLIP_CONTROL                },
    { STATE_VIEWPORT,                                     { STATE_VIEWPORT,                                     viewport_miscpart   }, WINED3D_GL_EXT_NONE             },
    { STATE_INDEXBUFFER,                                  { STATE_INDEXBUFFER,                                  indexbuffer         }, ARB_VERTEX_BUFFER_OBJECT        },
    { STATE_INDEXBUFFER,                                  { STATE_INDEXBUFFER,                                  state_nop           }, WINED3D_GL_EXT_NONE             },
    { STATE_RENDER(WINED3D_RS_LINEPATTERN),               { STATE_RENDER(WINED3D_RS_LINEPATTERN),               state_linepattern   }, WINED3D_GL_LEGACY_CONTEXT       },
    { STATE_RENDER(WINED3D_RS_LINEPATTERN),               { STATE_RENDER(WINED3D_RS_LINEPATTERN),               state_linepattern_w }, WINED3D_GL_EXT_NONE             },
    { STATE_RENDER(WINED3D_RS_DITHERENABLE),              { STATE_RENDER(WINED3D_RS_DITHERENABLE),              state_ditherenable  }, WINED3D_GL_EXT_NONE             },
    { STATE_RENDER(WINED3D_RS_MULTISAMPLEANTIALIAS),      { STATE_RENDER(WINED3D_RS_MULTISAMPLEANTIALIAS),      state_msaa          }, ARB_MULTISAMPLE                 },
    { STATE_RENDER(WINED3D_RS_MULTISAMPLEANTIALIAS),      { STATE_RENDER(WINED3D_RS_MULTISAMPLEANTIALIAS),      state_msaa_w        }, WINED3D_GL_EXT_NONE             },
    /* Samplers */
    { STATE_SAMPLER(0),                                   { STATE_SAMPLER(0),                                   sampler             }, WINED3D_GL_EXT_NONE             },
    { STATE_SAMPLER(1),                                   { STATE_SAMPLER(1),                                   sampler             }, WINED3D_GL_EXT_NONE             },
    { STATE_SAMPLER(2),                                   { STATE_SAMPLER(2),                                   sampler             }, WINED3D_GL_EXT_NONE             },
    { STATE_SAMPLER(3),                                   { STATE_SAMPLER(3),                                   sampler             }, WINED3D_GL_EXT_NONE             },
    { STATE_SAMPLER(4),                                   { STATE_SAMPLER(4),                                   sampler             }, WINED3D_GL_EXT_NONE             },
    { STATE_SAMPLER(5),                                   { STATE_SAMPLER(5),                                   sampler             }, WINED3D_GL_EXT_NONE             },
    { STATE_SAMPLER(6),                                   { STATE_SAMPLER(6),                                   sampler             }, WINED3D_GL_EXT_NONE             },
    { STATE_SAMPLER(7),                                   { STATE_SAMPLER(7),                                   sampler             }, WINED3D_GL_EXT_NONE             },
    { STATE_SAMPLER(8),                                   { STATE_SAMPLER(8),                                   sampler             }, WINED3D_GL_EXT_NONE             },
    { STATE_SAMPLER(9),                                   { STATE_SAMPLER(9),                                   sampler             }, WINED3D_GL_EXT_NONE             },
    { STATE_SAMPLER(10),                                  { STATE_SAMPLER(10),                                  sampler             }, WINED3D_GL_EXT_NONE             },
    { STATE_SAMPLER(11),                                  { STATE_SAMPLER(11),                                  sampler             }, WINED3D_GL_EXT_NONE             },
    { STATE_SAMPLER(12),                                  { STATE_SAMPLER(12),                                  sampler             }, WINED3D_GL_EXT_NONE             },
    { STATE_SAMPLER(13),                                  { STATE_SAMPLER(13),                                  sampler             }, WINED3D_GL_EXT_NONE             },
    { STATE_SAMPLER(14),                                  { STATE_SAMPLER(14),                                  sampler             }, WINED3D_GL_EXT_NONE             },
    { STATE_SAMPLER(15),                                  { STATE_SAMPLER(15),                                  sampler             }, WINED3D_GL_EXT_NONE             },
    { STATE_SAMPLER(16), /* Vertex sampler 0 */           { STATE_SAMPLER(16),                                  sampler             }, WINED3D_GL_EXT_NONE             },
    { STATE_SAMPLER(17), /* Vertex sampler 1 */           { STATE_SAMPLER(17),                                  sampler             }, WINED3D_GL_EXT_NONE             },
    { STATE_SAMPLER(18), /* Vertex sampler 2 */           { STATE_SAMPLER(18),                                  sampler             }, WINED3D_GL_EXT_NONE             },
    { STATE_SAMPLER(19), /* Vertex sampler 3 */           { STATE_SAMPLER(19),                                  sampler             }, WINED3D_GL_EXT_NONE             },
    { STATE_BASEVERTEXINDEX,                              { STATE_BASEVERTEXINDEX,                              state_nop,          }, ARB_DRAW_ELEMENTS_BASE_VERTEX   },
    { STATE_BASEVERTEXINDEX,                              { STATE_STREAMSRC,                                    NULL,               }, WINED3D_GL_EXT_NONE             },
    { STATE_FRAMEBUFFER,                                  { STATE_FRAMEBUFFER,                                  context_state_fb    }, WINED3D_GL_EXT_NONE             },
    { STATE_SHADER(WINED3D_SHADER_TYPE_PIXEL),            { STATE_SHADER(WINED3D_SHADER_TYPE_PIXEL),            context_state_drawbuf},WINED3D_GL_EXT_NONE             },
    { STATE_SHADER(WINED3D_SHADER_TYPE_HULL),             { STATE_SHADER(WINED3D_SHADER_TYPE_HULL),             state_shader        }, WINED3D_GL_EXT_NONE             },
    { STATE_SHADER(WINED3D_SHADER_TYPE_DOMAIN),           { STATE_SHADER(WINED3D_SHADER_TYPE_DOMAIN),           state_shader        }, WINED3D_GL_EXT_NONE             },
    { STATE_SHADER(WINED3D_SHADER_TYPE_GEOMETRY),         { STATE_SHADER(WINED3D_SHADER_TYPE_GEOMETRY),         state_shader        }, WINED3D_GL_EXT_NONE             },
    { STATE_SHADER(WINED3D_SHADER_TYPE_COMPUTE),          { STATE_SHADER(WINED3D_SHADER_TYPE_COMPUTE),          state_compute_shader}, WINED3D_GL_EXT_NONE             },
    {0 /* Terminate */,                                   { 0,                                                  0                   }, WINED3D_GL_EXT_NONE             },
};

static const struct wined3d_state_entry_template ffp_fragmentstate_template[] = {
    { STATE_TEXTURESTAGE(0, WINED3D_TSS_COLOR_OP),        { STATE_TEXTURESTAGE(0, WINED3D_TSS_COLOR_OP),        tex_colorop         }, WINED3D_GL_EXT_NONE             },
    { STATE_TEXTURESTAGE(0, WINED3D_TSS_COLOR_ARG1),      { STATE_TEXTURESTAGE(0, WINED3D_TSS_COLOR_OP),        NULL                }, WINED3D_GL_EXT_NONE             },
    { STATE_TEXTURESTAGE(0, WINED3D_TSS_COLOR_ARG2),      { STATE_TEXTURESTAGE(0, WINED3D_TSS_COLOR_OP),        NULL                }, WINED3D_GL_EXT_NONE             },
    { STATE_TEXTURESTAGE(0, WINED3D_TSS_ALPHA_OP),        { STATE_TEXTURESTAGE(0, WINED3D_TSS_ALPHA_OP),        tex_alphaop         }, WINED3D_GL_EXT_NONE             },
    { STATE_TEXTURESTAGE(0, WINED3D_TSS_ALPHA_ARG1),      { STATE_TEXTURESTAGE(0, WINED3D_TSS_ALPHA_OP),        NULL                }, WINED3D_GL_EXT_NONE             },
    { STATE_TEXTURESTAGE(0, WINED3D_TSS_ALPHA_ARG2),      { STATE_TEXTURESTAGE(0, WINED3D_TSS_ALPHA_OP),        NULL                }, WINED3D_GL_EXT_NONE             },
    { STATE_TEXTURESTAGE(0, WINED3D_TSS_COLOR_ARG0),      { STATE_TEXTURESTAGE(0, WINED3D_TSS_COLOR_OP),        NULL                }, WINED3D_GL_EXT_NONE             },
    { STATE_TEXTURESTAGE(0, WINED3D_TSS_ALPHA_ARG0),      { STATE_TEXTURESTAGE(0, WINED3D_TSS_ALPHA_OP),        NULL                }, WINED3D_GL_EXT_NONE             },
    { STATE_TEXTURESTAGE(0, WINED3D_TSS_RESULT_ARG),      { STATE_TEXTURESTAGE(0, WINED3D_TSS_COLOR_OP),        NULL                }, WINED3D_GL_EXT_NONE             },
    { STATE_TEXTURESTAGE(0, WINED3D_TSS_CONSTANT),        { 0 /* As long as we don't support D3DTA_CONSTANT */, NULL                }, WINED3D_GL_EXT_NONE             },
    { STATE_TEXTURESTAGE(1, WINED3D_TSS_COLOR_OP),        { STATE_TEXTURESTAGE(1, WINED3D_TSS_COLOR_OP),        tex_colorop         }, WINED3D_GL_EXT_NONE             },
    { STATE_TEXTURESTAGE(1, WINED3D_TSS_COLOR_ARG1),      { STATE_TEXTURESTAGE(1, WINED3D_TSS_COLOR_OP),        NULL                }, WINED3D_GL_EXT_NONE             },
    { STATE_TEXTURESTAGE(1, WINED3D_TSS_COLOR_ARG2),      { STATE_TEXTURESTAGE(1, WINED3D_TSS_COLOR_OP),        NULL                }, WINED3D_GL_EXT_NONE             },
    { STATE_TEXTURESTAGE(1, WINED3D_TSS_ALPHA_OP),        { STATE_TEXTURESTAGE(1, WINED3D_TSS_ALPHA_OP),        tex_alphaop         }, WINED3D_GL_EXT_NONE             },
    { STATE_TEXTURESTAGE(1, WINED3D_TSS_ALPHA_ARG1),      { STATE_TEXTURESTAGE(1, WINED3D_TSS_ALPHA_OP),        NULL                }, WINED3D_GL_EXT_NONE             },
    { STATE_TEXTURESTAGE(1, WINED3D_TSS_ALPHA_ARG2),      { STATE_TEXTURESTAGE(1, WINED3D_TSS_ALPHA_OP),        NULL                }, WINED3D_GL_EXT_NONE             },
    { STATE_TEXTURESTAGE(1, WINED3D_TSS_COLOR_ARG0),      { STATE_TEXTURESTAGE(1, WINED3D_TSS_COLOR_OP),        NULL                }, WINED3D_GL_EXT_NONE             },
    { STATE_TEXTURESTAGE(1, WINED3D_TSS_ALPHA_ARG0),      { STATE_TEXTURESTAGE(1, WINED3D_TSS_ALPHA_OP),        NULL                }, WINED3D_GL_EXT_NONE             },
    { STATE_TEXTURESTAGE(1, WINED3D_TSS_RESULT_ARG),      { STATE_TEXTURESTAGE(1, WINED3D_TSS_COLOR_OP),        NULL                }, WINED3D_GL_EXT_NONE             },
    { STATE_TEXTURESTAGE(1, WINED3D_TSS_CONSTANT),        { 0 /* As long as we don't support D3DTA_CONSTANT */, NULL                }, WINED3D_GL_EXT_NONE             },
    { STATE_TEXTURESTAGE(2, WINED3D_TSS_COLOR_OP),        { STATE_TEXTURESTAGE(2, WINED3D_TSS_COLOR_OP),        tex_colorop         }, WINED3D_GL_EXT_NONE             },
    { STATE_TEXTURESTAGE(2, WINED3D_TSS_COLOR_ARG1),      { STATE_TEXTURESTAGE(2, WINED3D_TSS_COLOR_OP),        NULL                }, WINED3D_GL_EXT_NONE             },
    { STATE_TEXTURESTAGE(2, WINED3D_TSS_COLOR_ARG2),      { STATE_TEXTURESTAGE(2, WINED3D_TSS_COLOR_OP),        NULL                }, WINED3D_GL_EXT_NONE             },
    { STATE_TEXTURESTAGE(2, WINED3D_TSS_ALPHA_OP),        { STATE_TEXTURESTAGE(2, WINED3D_TSS_ALPHA_OP),        tex_alphaop         }, WINED3D_GL_EXT_NONE             },
    { STATE_TEXTURESTAGE(2, WINED3D_TSS_ALPHA_ARG1),      { STATE_TEXTURESTAGE(2, WINED3D_TSS_ALPHA_OP),        NULL                }, WINED3D_GL_EXT_NONE             },
    { STATE_TEXTURESTAGE(2, WINED3D_TSS_ALPHA_ARG2),      { STATE_TEXTURESTAGE(2, WINED3D_TSS_ALPHA_OP),        NULL                }, WINED3D_GL_EXT_NONE             },
    { STATE_TEXTURESTAGE(2, WINED3D_TSS_COLOR_ARG0),      { STATE_TEXTURESTAGE(2, WINED3D_TSS_COLOR_OP),        NULL                }, WINED3D_GL_EXT_NONE             },
    { STATE_TEXTURESTAGE(2, WINED3D_TSS_ALPHA_ARG0),      { STATE_TEXTURESTAGE(2, WINED3D_TSS_ALPHA_OP),        NULL                }, WINED3D_GL_EXT_NONE             },
    { STATE_TEXTURESTAGE(2, WINED3D_TSS_RESULT_ARG),      { STATE_TEXTURESTAGE(2, WINED3D_TSS_COLOR_OP),        NULL                }, WINED3D_GL_EXT_NONE             },
    { STATE_TEXTURESTAGE(2, WINED3D_TSS_CONSTANT),        { 0 /* As long as we don't support D3DTA_CONSTANT */, NULL                }, WINED3D_GL_EXT_NONE             },
    { STATE_TEXTURESTAGE(3, WINED3D_TSS_COLOR_OP),        { STATE_TEXTURESTAGE(3, WINED3D_TSS_COLOR_OP),        tex_colorop         }, WINED3D_GL_EXT_NONE             },
    { STATE_TEXTURESTAGE(3, WINED3D_TSS_COLOR_ARG1),      { STATE_TEXTURESTAGE(3, WINED3D_TSS_COLOR_OP),        NULL                }, WINED3D_GL_EXT_NONE             },
    { STATE_TEXTURESTAGE(3, WINED3D_TSS_COLOR_ARG2),      { STATE_TEXTURESTAGE(3, WINED3D_TSS_COLOR_OP),        NULL                }, WINED3D_GL_EXT_NONE             },
    { STATE_TEXTURESTAGE(3, WINED3D_TSS_ALPHA_OP),        { STATE_TEXTURESTAGE(3, WINED3D_TSS_ALPHA_OP),        tex_alphaop         }, WINED3D_GL_EXT_NONE             },
    { STATE_TEXTURESTAGE(3, WINED3D_TSS_ALPHA_ARG1),      { STATE_TEXTURESTAGE(3, WINED3D_TSS_ALPHA_OP),        NULL                }, WINED3D_GL_EXT_NONE             },
    { STATE_TEXTURESTAGE(3, WINED3D_TSS_ALPHA_ARG2),      { STATE_TEXTURESTAGE(3, WINED3D_TSS_ALPHA_OP),        NULL                }, WINED3D_GL_EXT_NONE             },
    { STATE_TEXTURESTAGE(3, WINED3D_TSS_COLOR_ARG0),      { STATE_TEXTURESTAGE(3, WINED3D_TSS_COLOR_OP),        NULL                }, WINED3D_GL_EXT_NONE             },
    { STATE_TEXTURESTAGE(3, WINED3D_TSS_ALPHA_ARG0),      { STATE_TEXTURESTAGE(3, WINED3D_TSS_ALPHA_OP),        NULL                }, WINED3D_GL_EXT_NONE             },
    { STATE_TEXTURESTAGE(3, WINED3D_TSS_RESULT_ARG),      { STATE_TEXTURESTAGE(3, WINED3D_TSS_COLOR_OP),        NULL                }, WINED3D_GL_EXT_NONE             },
    { STATE_TEXTURESTAGE(3, WINED3D_TSS_CONSTANT),        { 0 /* As long as we don't support D3DTA_CONSTANT */, NULL                }, WINED3D_GL_EXT_NONE             },
    { STATE_TEXTURESTAGE(4, WINED3D_TSS_COLOR_OP),        { STATE_TEXTURESTAGE(4, WINED3D_TSS_COLOR_OP),        tex_colorop         }, WINED3D_GL_EXT_NONE             },
    { STATE_TEXTURESTAGE(4, WINED3D_TSS_COLOR_ARG1),      { STATE_TEXTURESTAGE(4, WINED3D_TSS_COLOR_OP),        NULL                }, WINED3D_GL_EXT_NONE             },
    { STATE_TEXTURESTAGE(4, WINED3D_TSS_COLOR_ARG2),      { STATE_TEXTURESTAGE(4, WINED3D_TSS_COLOR_OP),        NULL                }, WINED3D_GL_EXT_NONE             },
    { STATE_TEXTURESTAGE(4, WINED3D_TSS_ALPHA_OP),        { STATE_TEXTURESTAGE(4, WINED3D_TSS_ALPHA_OP),        tex_alphaop         }, WINED3D_GL_EXT_NONE             },
    { STATE_TEXTURESTAGE(4, WINED3D_TSS_ALPHA_ARG1),      { STATE_TEXTURESTAGE(4, WINED3D_TSS_ALPHA_OP),        NULL                }, WINED3D_GL_EXT_NONE             },
    { STATE_TEXTURESTAGE(4, WINED3D_TSS_ALPHA_ARG2),      { STATE_TEXTURESTAGE(4, WINED3D_TSS_ALPHA_OP),        NULL                }, WINED3D_GL_EXT_NONE             },
    { STATE_TEXTURESTAGE(4, WINED3D_TSS_COLOR_ARG0),      { STATE_TEXTURESTAGE(4, WINED3D_TSS_COLOR_OP),        NULL                }, WINED3D_GL_EXT_NONE             },
    { STATE_TEXTURESTAGE(4, WINED3D_TSS_ALPHA_ARG0),      { STATE_TEXTURESTAGE(4, WINED3D_TSS_ALPHA_OP),        NULL                }, WINED3D_GL_EXT_NONE             },
    { STATE_TEXTURESTAGE(4, WINED3D_TSS_RESULT_ARG),      { STATE_TEXTURESTAGE(4, WINED3D_TSS_COLOR_OP),        NULL                }, WINED3D_GL_EXT_NONE             },
    { STATE_TEXTURESTAGE(4, WINED3D_TSS_CONSTANT),        { 0 /* As long as we don't support D3DTA_CONSTANT */, NULL                }, WINED3D_GL_EXT_NONE             },
    { STATE_TEXTURESTAGE(5, WINED3D_TSS_COLOR_OP),        { STATE_TEXTURESTAGE(5, WINED3D_TSS_COLOR_OP),        tex_colorop         }, WINED3D_GL_EXT_NONE             },
    { STATE_TEXTURESTAGE(5, WINED3D_TSS_COLOR_ARG1),      { STATE_TEXTURESTAGE(5, WINED3D_TSS_COLOR_OP),        NULL                }, WINED3D_GL_EXT_NONE             },
    { STATE_TEXTURESTAGE(5, WINED3D_TSS_COLOR_ARG2),      { STATE_TEXTURESTAGE(5, WINED3D_TSS_COLOR_OP),        NULL                }, WINED3D_GL_EXT_NONE             },
    { STATE_TEXTURESTAGE(5, WINED3D_TSS_ALPHA_OP),        { STATE_TEXTURESTAGE(5, WINED3D_TSS_ALPHA_OP),        tex_alphaop         }, WINED3D_GL_EXT_NONE             },
    { STATE_TEXTURESTAGE(5, WINED3D_TSS_ALPHA_ARG1),      { STATE_TEXTURESTAGE(5, WINED3D_TSS_ALPHA_OP),        NULL                }, WINED3D_GL_EXT_NONE             },
    { STATE_TEXTURESTAGE(5, WINED3D_TSS_ALPHA_ARG2),      { STATE_TEXTURESTAGE(5, WINED3D_TSS_ALPHA_OP),        NULL                }, WINED3D_GL_EXT_NONE             },
    { STATE_TEXTURESTAGE(5, WINED3D_TSS_COLOR_ARG0),      { STATE_TEXTURESTAGE(5, WINED3D_TSS_COLOR_OP),        NULL                }, WINED3D_GL_EXT_NONE             },
    { STATE_TEXTURESTAGE(5, WINED3D_TSS_ALPHA_ARG0),      { STATE_TEXTURESTAGE(5, WINED3D_TSS_ALPHA_OP),        NULL                }, WINED3D_GL_EXT_NONE             },
    { STATE_TEXTURESTAGE(5, WINED3D_TSS_RESULT_ARG),      { STATE_TEXTURESTAGE(5, WINED3D_TSS_COLOR_OP),        NULL                }, WINED3D_GL_EXT_NONE             },
    { STATE_TEXTURESTAGE(5, WINED3D_TSS_CONSTANT),        { 0 /* As long as we don't support D3DTA_CONSTANT */, NULL                }, WINED3D_GL_EXT_NONE             },
    { STATE_TEXTURESTAGE(6, WINED3D_TSS_COLOR_OP),        { STATE_TEXTURESTAGE(6, WINED3D_TSS_COLOR_OP),        tex_colorop         }, WINED3D_GL_EXT_NONE             },
    { STATE_TEXTURESTAGE(6, WINED3D_TSS_COLOR_ARG1),      { STATE_TEXTURESTAGE(6, WINED3D_TSS_COLOR_OP),        NULL                }, WINED3D_GL_EXT_NONE             },
    { STATE_TEXTURESTAGE(6, WINED3D_TSS_COLOR_ARG2),      { STATE_TEXTURESTAGE(6, WINED3D_TSS_COLOR_OP),        NULL                }, WINED3D_GL_EXT_NONE             },
    { STATE_TEXTURESTAGE(6, WINED3D_TSS_ALPHA_OP),        { STATE_TEXTURESTAGE(6, WINED3D_TSS_ALPHA_OP),        tex_alphaop         }, WINED3D_GL_EXT_NONE             },
    { STATE_TEXTURESTAGE(6, WINED3D_TSS_ALPHA_ARG1),      { STATE_TEXTURESTAGE(6, WINED3D_TSS_ALPHA_OP),        NULL                }, WINED3D_GL_EXT_NONE             },
    { STATE_TEXTURESTAGE(6, WINED3D_TSS_ALPHA_ARG2),      { STATE_TEXTURESTAGE(6, WINED3D_TSS_ALPHA_OP),        NULL                }, WINED3D_GL_EXT_NONE             },
    { STATE_TEXTURESTAGE(6, WINED3D_TSS_COLOR_ARG0),      { STATE_TEXTURESTAGE(6, WINED3D_TSS_COLOR_OP),        NULL                }, WINED3D_GL_EXT_NONE             },
    { STATE_TEXTURESTAGE(6, WINED3D_TSS_ALPHA_ARG0),      { STATE_TEXTURESTAGE(6, WINED3D_TSS_ALPHA_OP),        NULL                }, WINED3D_GL_EXT_NONE             },
    { STATE_TEXTURESTAGE(6, WINED3D_TSS_RESULT_ARG),      { STATE_TEXTURESTAGE(6, WINED3D_TSS_COLOR_OP),        NULL                }, WINED3D_GL_EXT_NONE             },
    { STATE_TEXTURESTAGE(6, WINED3D_TSS_CONSTANT),        { 0 /* As long as we don't support D3DTA_CONSTANT */, NULL                }, WINED3D_GL_EXT_NONE             },
    { STATE_TEXTURESTAGE(7, WINED3D_TSS_COLOR_OP),        { STATE_TEXTURESTAGE(7, WINED3D_TSS_COLOR_OP),        tex_colorop         }, WINED3D_GL_EXT_NONE             },
    { STATE_TEXTURESTAGE(7, WINED3D_TSS_COLOR_ARG1),      { STATE_TEXTURESTAGE(7, WINED3D_TSS_COLOR_OP),        NULL                }, WINED3D_GL_EXT_NONE             },
    { STATE_TEXTURESTAGE(7, WINED3D_TSS_COLOR_ARG2),      { STATE_TEXTURESTAGE(7, WINED3D_TSS_COLOR_OP),        NULL                }, WINED3D_GL_EXT_NONE             },
    { STATE_TEXTURESTAGE(7, WINED3D_TSS_ALPHA_OP),        { STATE_TEXTURESTAGE(7, WINED3D_TSS_ALPHA_OP),        tex_alphaop         }, WINED3D_GL_EXT_NONE             },
    { STATE_TEXTURESTAGE(7, WINED3D_TSS_ALPHA_ARG1),      { STATE_TEXTURESTAGE(7, WINED3D_TSS_ALPHA_OP),        NULL                }, WINED3D_GL_EXT_NONE             },
    { STATE_TEXTURESTAGE(7, WINED3D_TSS_ALPHA_ARG2),      { STATE_TEXTURESTAGE(7, WINED3D_TSS_ALPHA_OP),        NULL                }, WINED3D_GL_EXT_NONE             },
    { STATE_TEXTURESTAGE(7, WINED3D_TSS_COLOR_ARG0),      { STATE_TEXTURESTAGE(7, WINED3D_TSS_COLOR_OP),        NULL                }, WINED3D_GL_EXT_NONE             },
    { STATE_TEXTURESTAGE(7, WINED3D_TSS_ALPHA_ARG0),      { STATE_TEXTURESTAGE(7, WINED3D_TSS_ALPHA_OP),        NULL                }, WINED3D_GL_EXT_NONE             },
    { STATE_TEXTURESTAGE(7, WINED3D_TSS_RESULT_ARG),      { STATE_TEXTURESTAGE(7, WINED3D_TSS_COLOR_OP),        NULL                }, WINED3D_GL_EXT_NONE             },
    { STATE_TEXTURESTAGE(7, WINED3D_TSS_CONSTANT),        { 0 /* As long as we don't support D3DTA_CONSTANT */, NULL                }, WINED3D_GL_EXT_NONE             },
    { STATE_SHADER(WINED3D_SHADER_TYPE_PIXEL),            { STATE_SHADER(WINED3D_SHADER_TYPE_PIXEL),            apply_pixelshader   }, WINED3D_GL_EXT_NONE             },
    { STATE_RENDER(WINED3D_RS_ALPHAFUNC),                 { STATE_RENDER(WINED3D_RS_ALPHATESTENABLE),           NULL                }, WINED3D_GL_EXT_NONE             },
    { STATE_RENDER(WINED3D_RS_ALPHAREF),                  { STATE_RENDER(WINED3D_RS_ALPHATESTENABLE),           NULL                }, WINED3D_GL_EXT_NONE             },
    { STATE_RENDER(WINED3D_RS_ALPHATESTENABLE),           { STATE_RENDER(WINED3D_RS_ALPHATESTENABLE),           state_alpha_test    }, WINED3D_GL_EXT_NONE             },
    { STATE_RENDER(WINED3D_RS_COLORKEYENABLE),            { STATE_RENDER(WINED3D_RS_ALPHATESTENABLE),           NULL                }, WINED3D_GL_EXT_NONE             },
    { STATE_COLOR_KEY,                                    { STATE_COLOR_KEY,                                    state_nop           }, WINED3D_GL_EXT_NONE             },
    { STATE_RENDER(WINED3D_RS_SRGBWRITEENABLE),           { STATE_SHADER(WINED3D_SHADER_TYPE_PIXEL),            NULL                }, WINED3D_GL_EXT_NONE             },
    { STATE_RENDER(WINED3D_RS_TEXTUREFACTOR),             { STATE_RENDER(WINED3D_RS_TEXTUREFACTOR),             state_texfactor     }, WINED3D_GL_EXT_NONE             },
    { STATE_RENDER(WINED3D_RS_FOGCOLOR),                  { STATE_RENDER(WINED3D_RS_FOGCOLOR),                  state_fogcolor      }, WINED3D_GL_EXT_NONE             },
    { STATE_RENDER(WINED3D_RS_FOGDENSITY),                { STATE_RENDER(WINED3D_RS_FOGDENSITY),                state_fogdensity    }, WINED3D_GL_EXT_NONE             },
    { STATE_RENDER(WINED3D_RS_FOGENABLE),                 { STATE_RENDER(WINED3D_RS_FOGENABLE),                 state_fog_fragpart  }, WINED3D_GL_EXT_NONE             },
    { STATE_RENDER(WINED3D_RS_FOGTABLEMODE),              { STATE_RENDER(WINED3D_RS_FOGENABLE),                 NULL                }, WINED3D_GL_EXT_NONE             },
    { STATE_RENDER(WINED3D_RS_FOGVERTEXMODE),             { STATE_RENDER(WINED3D_RS_FOGENABLE),                 NULL                }, WINED3D_GL_EXT_NONE             },
    { STATE_RENDER(WINED3D_RS_FOGSTART),                  { STATE_RENDER(WINED3D_RS_FOGSTART),                  state_fogstartend   }, WINED3D_GL_EXT_NONE             },
    { STATE_RENDER(WINED3D_RS_FOGEND),                    { STATE_RENDER(WINED3D_RS_FOGSTART),                  NULL                }, WINED3D_GL_EXT_NONE             },
    { STATE_RENDER(WINED3D_RS_SRGBWRITEENABLE),           { STATE_RENDER(WINED3D_RS_SRGBWRITEENABLE),           state_srgbwrite     }, ARB_FRAMEBUFFER_SRGB            },
    { STATE_RENDER(WINED3D_RS_SHADEMODE),                 { STATE_RENDER(WINED3D_RS_SHADEMODE),                 state_shademode     }, WINED3D_GL_EXT_NONE             },
    { STATE_SAMPLER(0),                                   { STATE_SAMPLER(0),                                   sampler_texdim      }, WINED3D_GL_EXT_NONE             },
    { STATE_SAMPLER(1),                                   { STATE_SAMPLER(1),                                   sampler_texdim      }, WINED3D_GL_EXT_NONE             },
    { STATE_SAMPLER(2),                                   { STATE_SAMPLER(2),                                   sampler_texdim      }, WINED3D_GL_EXT_NONE             },
    { STATE_SAMPLER(3),                                   { STATE_SAMPLER(3),                                   sampler_texdim      }, WINED3D_GL_EXT_NONE             },
    { STATE_SAMPLER(4),                                   { STATE_SAMPLER(4),                                   sampler_texdim      }, WINED3D_GL_EXT_NONE             },
    { STATE_SAMPLER(5),                                   { STATE_SAMPLER(5),                                   sampler_texdim      }, WINED3D_GL_EXT_NONE             },
    { STATE_SAMPLER(6),                                   { STATE_SAMPLER(6),                                   sampler_texdim      }, WINED3D_GL_EXT_NONE             },
    { STATE_SAMPLER(7),                                   { STATE_SAMPLER(7),                                   sampler_texdim      }, WINED3D_GL_EXT_NONE             },
    {0 /* Terminate */,                                   { 0,                                                  0                   }, WINED3D_GL_EXT_NONE             },
};

/* Context activation is done by the caller. */
static void ffp_pipe_apply_draw_state(struct wined3d_context *context, const struct wined3d_state *state) {}

static void ffp_pipe_disable(const struct wined3d_context *context) {}

static void *ffp_alloc(const struct wined3d_shader_backend_ops *shader_backend, void *shader_priv)
{
    return shader_priv;
}

static void ffp_free(struct wined3d_device *device, struct wined3d_context *context) {}

static void ffp_fragment_get_caps(const struct wined3d_adapter *adapter, struct fragment_caps *caps)
{
    const struct wined3d_gl_info *gl_info = &wined3d_adapter_gl_const(adapter)->gl_info;

    memset(caps, 0, sizeof(*caps));
    caps->PrimitiveMiscCaps = 0;
    caps->TextureOpCaps = WINED3DTEXOPCAPS_ADD
            | WINED3DTEXOPCAPS_ADDSIGNED
            | WINED3DTEXOPCAPS_ADDSIGNED2X
            | WINED3DTEXOPCAPS_MODULATE
            | WINED3DTEXOPCAPS_MODULATE2X
            | WINED3DTEXOPCAPS_MODULATE4X
            | WINED3DTEXOPCAPS_SELECTARG1
            | WINED3DTEXOPCAPS_SELECTARG2
            | WINED3DTEXOPCAPS_DISABLE;
    caps->srgb_write = !!gl_info->supported[ARB_FRAMEBUFFER_SRGB];

    if (gl_info->supported[ARB_TEXTURE_ENV_COMBINE]
            || gl_info->supported[EXT_TEXTURE_ENV_COMBINE]
            || gl_info->supported[NV_TEXTURE_ENV_COMBINE4])
    {
        caps->TextureOpCaps |= WINED3DTEXOPCAPS_BLENDDIFFUSEALPHA
                | WINED3DTEXOPCAPS_BLENDTEXTUREALPHA
                | WINED3DTEXOPCAPS_BLENDFACTORALPHA
                | WINED3DTEXOPCAPS_BLENDCURRENTALPHA
                | WINED3DTEXOPCAPS_LERP
                | WINED3DTEXOPCAPS_SUBTRACT;
    }
    if (gl_info->supported[ATI_TEXTURE_ENV_COMBINE3]
            || gl_info->supported[NV_TEXTURE_ENV_COMBINE4])
    {
        caps->TextureOpCaps |= WINED3DTEXOPCAPS_ADDSMOOTH
                | WINED3DTEXOPCAPS_MULTIPLYADD
                | WINED3DTEXOPCAPS_MODULATEALPHA_ADDCOLOR
                | WINED3DTEXOPCAPS_MODULATECOLOR_ADDALPHA
                | WINED3DTEXOPCAPS_BLENDTEXTUREALPHAPM;
    }
    if (gl_info->supported[ARB_TEXTURE_ENV_DOT3])
        caps->TextureOpCaps |= WINED3DTEXOPCAPS_DOTPRODUCT3;

    caps->max_blend_stages = gl_info->limits.ffp_textures;
    caps->max_textures = gl_info->limits.ffp_textures;
}

static unsigned int ffp_fragment_get_emul_mask(const struct wined3d_adapter *adapter)
{
    return GL_EXT_EMUL_ARB_MULTITEXTURE | GL_EXT_EMUL_EXT_FOG_COORD;
}

static BOOL ffp_color_fixup_supported(struct color_fixup_desc fixup)
{
    /* We only support identity conversions. */
    return is_identity_fixup(fixup);
}

static BOOL ffp_none_context_alloc(struct wined3d_context *context)
{
    return TRUE;
}

static void ffp_none_context_free(struct wined3d_context *context)
{
}

const struct wined3d_fragment_pipe_ops ffp_fragment_pipeline =
{
    .fp_apply_draw_state = ffp_pipe_apply_draw_state,
    .fp_disable = ffp_pipe_disable,
    .get_caps = ffp_fragment_get_caps,
    .get_emul_mask = ffp_fragment_get_emul_mask,
    .alloc_private = ffp_alloc,
    .free_private = ffp_free,
    .allocate_context_data = ffp_none_context_alloc,
    .free_context_data = ffp_none_context_free,
    .color_fixup_supported = ffp_color_fixup_supported,
    .states = ffp_fragmentstate_template,
};

static void none_pipe_apply_draw_state(struct wined3d_context *context, const struct wined3d_state *state) {}

static void none_pipe_disable(const struct wined3d_context *context) {}

static void *none_alloc(const struct wined3d_shader_backend_ops *shader_backend, void *shader_priv)
{
    return shader_priv;
}

static void none_free(struct wined3d_device *device, struct wined3d_context *context) {}

static void vp_none_get_caps(const struct wined3d_adapter *adapter, struct wined3d_vertex_caps *caps)
{
    memset(caps, 0, sizeof(*caps));
}

static unsigned int vp_none_get_emul_mask(const struct wined3d_adapter *adapter)
{
    return 0;
}

const struct wined3d_vertex_pipe_ops none_vertex_pipe =
{
    .vp_apply_draw_state = none_pipe_apply_draw_state,
    .vp_disable = none_pipe_disable,
    .vp_get_caps = vp_none_get_caps,
    .vp_get_emul_mask = vp_none_get_emul_mask,
    .vp_alloc = none_alloc,
    .vp_free = none_free,
};

static void fp_none_get_caps(const struct wined3d_adapter *adapter, struct fragment_caps *caps)
{
    memset(caps, 0, sizeof(*caps));
}

static unsigned int fp_none_get_emul_mask(const struct wined3d_adapter *adapter)
{
    return 0;
}

static BOOL fp_none_color_fixup_supported(struct color_fixup_desc fixup)
{
    return is_identity_fixup(fixup);
}

const struct wined3d_fragment_pipe_ops none_fragment_pipe =
{
    .fp_apply_draw_state = none_pipe_apply_draw_state,
    .fp_disable = none_pipe_disable,
    .get_caps = fp_none_get_caps,
    .get_emul_mask = fp_none_get_emul_mask,
    .alloc_private = none_alloc,
    .free_private = none_free,
    .allocate_context_data = ffp_none_context_alloc,
    .free_context_data = ffp_none_context_free,
    .color_fixup_supported = fp_none_color_fixup_supported,
};

static unsigned int num_handlers(const APPLYSTATEFUNC *funcs)
{
    unsigned int i;
    for(i = 0; funcs[i]; i++);
    return i;
}

static void multistate_apply_2(struct wined3d_context *context, const struct wined3d_state *state, DWORD state_id)
{
    context->device->multistate_funcs[state_id][0](context, state, state_id);
    context->device->multistate_funcs[state_id][1](context, state, state_id);
}

static void multistate_apply_3(struct wined3d_context *context, const struct wined3d_state *state, DWORD state_id)
{
    context->device->multistate_funcs[state_id][0](context, state, state_id);
    context->device->multistate_funcs[state_id][1](context, state, state_id);
    context->device->multistate_funcs[state_id][2](context, state, state_id);
}

static void prune_invalid_states(struct wined3d_state_entry *state_table, const struct wined3d_d3d_info *d3d_info)
{
    unsigned int start, last, i;

    start = STATE_TEXTURESTAGE(d3d_info->ffp_fragment_caps.max_blend_stages, 0);
    last = STATE_TEXTURESTAGE(WINED3D_MAX_FFP_TEXTURES - 1, WINED3D_HIGHEST_TEXTURE_STATE);
    for (i = start; i <= last; ++i)
    {
        state_table[i].representative = 0;
        state_table[i].apply = state_undefined;
    }

    start = STATE_TRANSFORM(WINED3D_TS_TEXTURE0 + d3d_info->ffp_fragment_caps.max_blend_stages);
    last = STATE_TRANSFORM(WINED3D_TS_TEXTURE0 + WINED3D_MAX_FFP_TEXTURES - 1);
    for (i = start; i <= last; ++i)
    {
        state_table[i].representative = 0;
        state_table[i].apply = state_undefined;
    }

    start = STATE_TRANSFORM(WINED3D_TS_WORLD_MATRIX(d3d_info->limits.ffp_vertex_blend_matrices));
    last = STATE_TRANSFORM(WINED3D_TS_WORLD_MATRIX(255));
    for (i = start; i <= last; ++i)
    {
        state_table[i].representative = 0;
        state_table[i].apply = state_undefined;
    }
}

static void validate_state_table(struct wined3d_state_entry *state_table)
{
    static const struct
    {
        DWORD first;
        DWORD last;
    }
    rs_holes[] =
    {
        {  1,   8},
        { 11,  14},
        { 16,  23},
        { 27,  27},
        { 30,  33},
        { 39,  40},
        { 42,  47},
        { 49,  59},
        { 61, 135},
        {138, 138},
        {144, 144},
        {149, 150},
        {153, 153},
        {162, 165},
        {167, 193},
        {195, 209},
        {  0,   0},
    };
    static const unsigned int simple_states[] =
    {
        STATE_MATERIAL,
        STATE_VDECL,
        STATE_STREAMSRC,
        STATE_INDEXBUFFER,
        STATE_SHADER(WINED3D_SHADER_TYPE_VERTEX),
        STATE_SHADER(WINED3D_SHADER_TYPE_HULL),
        STATE_SHADER(WINED3D_SHADER_TYPE_DOMAIN),
        STATE_SHADER(WINED3D_SHADER_TYPE_GEOMETRY),
        STATE_SHADER(WINED3D_SHADER_TYPE_PIXEL),
        STATE_SHADER(WINED3D_SHADER_TYPE_COMPUTE),
        STATE_CONSTANT_BUFFER(WINED3D_SHADER_TYPE_VERTEX),
        STATE_CONSTANT_BUFFER(WINED3D_SHADER_TYPE_HULL),
        STATE_CONSTANT_BUFFER(WINED3D_SHADER_TYPE_DOMAIN),
        STATE_CONSTANT_BUFFER(WINED3D_SHADER_TYPE_GEOMETRY),
        STATE_CONSTANT_BUFFER(WINED3D_SHADER_TYPE_PIXEL),
        STATE_CONSTANT_BUFFER(WINED3D_SHADER_TYPE_COMPUTE),
        STATE_COMPUTE_SHADER_RESOURCE_BINDING,
        STATE_GRAPHICS_SHADER_RESOURCE_BINDING,
        STATE_COMPUTE_UNORDERED_ACCESS_VIEW_BINDING,
        STATE_GRAPHICS_UNORDERED_ACCESS_VIEW_BINDING,
        STATE_VIEWPORT,
        STATE_LIGHT_TYPE,
        STATE_SCISSORRECT,
        STATE_RASTERIZER,
        STATE_POINTSPRITECOORDORIGIN,
        STATE_BASEVERTEXINDEX,
        STATE_FRAMEBUFFER,
        STATE_POINT_ENABLE,
        STATE_COLOR_KEY,
        STATE_BLEND,
        STATE_BLEND_FACTOR,
        STATE_DEPTH_STENCIL,
        STATE_STENCIL_REF,
        STATE_DEPTH_BOUNDS,
    };
    unsigned int i, current;

    for (i = STATE_RENDER(1), current = 0; i <= STATE_RENDER(WINEHIGHEST_RENDER_STATE); ++i)
    {
        if (!rs_holes[current].first || i < STATE_RENDER(rs_holes[current].first))
        {
            if (!state_table[i].representative)
                ERR("State %s (%#x) should have a representative.\n", debug_d3dstate(i), i);
        }
        else if (state_table[i].representative)
            ERR("State %s (%#x) shouldn't have a representative.\n", debug_d3dstate(i), i);

        if (i == STATE_RENDER(rs_holes[current].last)) ++current;
    }

    for (i = 0; i < ARRAY_SIZE(simple_states); ++i)
    {
        if (!state_table[simple_states[i]].representative)
            ERR("State %s (%#x) should have a representative.\n",
                    debug_d3dstate(simple_states[i]), simple_states[i]);
    }

    for (i = 0; i < STATE_HIGHEST + 1; ++i)
    {
        unsigned int rep = state_table[i].representative;
        if (rep)
        {
            if (state_table[rep].representative != rep)
            {
                ERR("State %s (%#x) has invalid representative %s (%#x).\n",
                        debug_d3dstate(i), i, debug_d3dstate(rep), rep);
                state_table[i].representative = 0;
            }

            if (rep != i)
            {
                if (state_table[i].apply)
                    ERR("State %s (%#x) has both a handler and representative.\n", debug_d3dstate(i), i);
            }
            else if (!state_table[i].apply)
            {
                ERR("Self representing state %s (%#x) has no handler.\n", debug_d3dstate(i), i);
            }
        }
    }
}

HRESULT compile_state_table(struct wined3d_state_entry *state_table, APPLYSTATEFUNC **dev_multistate_funcs,
        const struct wined3d_d3d_info *d3d_info, const BOOL *supported_extensions,
        const struct wined3d_vertex_pipe_ops *vertex, const struct wined3d_fragment_pipe_ops *fragment,
        const struct wined3d_state_entry_template *misc)
{
    APPLYSTATEFUNC multistate_funcs[STATE_HIGHEST + 1][3];
    const struct wined3d_state_entry_template *cur;
    unsigned int i, type, handlers;
    BOOL set[STATE_HIGHEST + 1];

    memset(multistate_funcs, 0, sizeof(multistate_funcs));

    for (i = 0; i < STATE_HIGHEST + 1; ++i)
    {
        state_table[i].representative = 0;
        state_table[i].apply = state_undefined;
    }

    for (type = 0; type < 3; ++type)
    {
        /* This switch decides the order in which the states are applied */
        switch (type)
        {
            case 0: cur = misc; break;
            case 1: cur = fragment->states; break;
            case 2: cur = vertex->vp_states; break;
            default: cur = NULL; /* Stupid compiler */
        }
        if (!cur) continue;

        /* GL extension filtering should not prevent multiple handlers being applied from different
         * pipeline parts
         */
        memset(set, 0, sizeof(set));

        for (i = 0; cur[i].state; ++i)
        {
            APPLYSTATEFUNC *funcs_array;

            /* Only use the first matching state with the available extension from one template.
             * e.g.
             * {D3DRS_FOOBAR, {D3DRS_FOOBAR, func1}, XYZ_FANCY},
             * {D3DRS_FOOBAR, {D3DRS_FOOBAR, func2}, 0        }
             *
             * if GL_XYZ_fancy is supported, ignore the 2nd line
             */
            if (set[cur[i].state]) continue;
            /* Skip state lines depending on unsupported extensions */
            if (!supported_extensions[cur[i].extension]) continue;
            set[cur[i].state] = TRUE;
            /* In some cases having an extension means that nothing has to be
             * done for a state, e.g. if GL_ARB_texture_non_power_of_two is
             * supported, the texture coordinate fixup can be ignored. If the
             * apply function is used, mark the state set(done above) to prevent
             * applying later lines, but do not record anything in the state
             * table
             */
            if (!cur[i].content.representative) continue;

            handlers = num_handlers(multistate_funcs[cur[i].state]);
            multistate_funcs[cur[i].state][handlers] = cur[i].content.apply;
            switch (handlers)
            {
                case 0:
                    state_table[cur[i].state].apply = cur[i].content.apply;
                    break;
                case 1:
                    state_table[cur[i].state].apply = multistate_apply_2;
                    if (!(dev_multistate_funcs[cur[i].state] = calloc(2, sizeof(**dev_multistate_funcs))))
                        goto out_of_mem;

                    dev_multistate_funcs[cur[i].state][0] = multistate_funcs[cur[i].state][0];
                    dev_multistate_funcs[cur[i].state][1] = multistate_funcs[cur[i].state][1];
                    break;
                case 2:
                    state_table[cur[i].state].apply = multistate_apply_3;
                    if (!(funcs_array = realloc(dev_multistate_funcs[cur[i].state],
                            sizeof(**dev_multistate_funcs) * 3)))
                        goto out_of_mem;

                    dev_multistate_funcs[cur[i].state] = funcs_array;
                    dev_multistate_funcs[cur[i].state][2] = multistate_funcs[cur[i].state][2];
                    break;
                default:
                    ERR("Unexpected amount of state handlers for state %u: %u.\n",
                            cur[i].state, handlers + 1);
            }

            if (state_table[cur[i].state].representative
                    && state_table[cur[i].state].representative != cur[i].content.representative)
            {
                FIXME("State %s (%#x) has different representatives in different pipeline parts.\n",
                        debug_d3dstate(cur[i].state), cur[i].state);
            }
            state_table[cur[i].state].representative = cur[i].content.representative;
        }
    }

    prune_invalid_states(state_table, d3d_info);
    validate_state_table(state_table);

    return WINED3D_OK;

out_of_mem:
    for (i = 0; i <= STATE_HIGHEST; ++i)
    {
        free(dev_multistate_funcs[i]);
    }

    memset(dev_multistate_funcs, 0, (STATE_HIGHEST + 1) * sizeof(*dev_multistate_funcs));

    return E_OUTOFMEMORY;
}
