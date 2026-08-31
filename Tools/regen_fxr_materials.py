# Copyright (c) 2026 Low Sze Hao. All rights reserved.
#
# Regenerates the FlexXR presentation materials from source, so plugin content is
# reproducible rather than hand-authored:
#
#   M_FXR_Outline           highlight outline post-process (reads the packed stencil)
#   M_FXR_HighlightOverlay  Inner Blink / Sweep, drawn per mesh through the overlay slot
#   M_FXR_Ghost             FXR_Socket placement preview
#   M_FXR_Ray               far-interaction pointer beam
#
# Run from the editor console:  py "G:/Flex_XR_Framework/Tools/regen_fxr_materials.py"

import unreal

PATH = '/FlexXR/Materials'
mel = unreal.MaterialEditingLibrary
fails = []


def clear_expressions(mat):
    """Empty a material's graph, and verify it actually emptied.

    delete_all_material_expressions silently leaves nodes behind. Every rebuild then stacked a fresh
    copy on top of the survivors while the material's inputs stayed wired to the *old* chain — so the
    graph grew duplicate parameters, edits appeared to do nothing, and a material could report
    "rebuilt" with zero errors while rendering from a version nobody had authored. Deleting one by
    one afterwards is what actually clears it.
    """
    try:
        mel.delete_all_material_expressions(mat)
    except Exception:
        pass

    for _ in range(4):
        try:
            collection = mat.get_editor_property('expression_collection')
            remaining = list(collection.get_editor_property('expressions'))
        except Exception:
            return
        if not remaining:
            return
        for expression in remaining:
            if expression:
                try:
                    mel.delete_material_expression(mat, expression)
                except Exception:
                    pass


def new_material(name, blend=None, shading=None, domain=None, two_sided=False):
    """Recreate the material from scratch, falling back to clearing it in place."""
    full = '%s/%s' % (PATH, name)

    # Deleted and recreated, so the graph is provably empty. Rebuilding in place looked like the
    # careful option and was the opposite: clearing leaves nodes behind, every rebuild stacked a
    # fresh copy on the survivors, and the material's inputs stayed wired to the *old* chain — so
    # edits appeared to do nothing and a material rendered from a version nobody had authored.
    #
    # Safe only because every reference to these is now soft. A hard reference from a CDO roots the
    # asset, and deleting a rooted asset takes the editor down: that is exactly what happened when
    # UFXR_Socket held M_FXR_Ghost through ConstructorHelpers. If that ever returns, this crashes
    # again — hence the fallback below rather than an assumption.
    if unreal.EditorAssetLibrary.does_asset_exist(full):
        try:
            unreal.EditorAssetLibrary.delete_asset(full)
        except Exception:
            pass

    if unreal.EditorAssetLibrary.does_asset_exist(full):
        mat = unreal.EditorAssetLibrary.load_asset(full)
        clear_expressions(mat)
    else:
        tools = unreal.AssetToolsHelpers.get_asset_tools()
        mat = tools.create_asset(name, PATH, unreal.Material, unreal.MaterialFactoryNew())
    if domain is not None:
        mat.set_editor_property('material_domain', domain)
    if blend is not None:
        mat.set_editor_property('blend_mode', blend)
    if shading is not None:
        mat.set_editor_property('shading_model', shading)
    if two_sided:
        mat.set_editor_property('two_sided', True)
    return mat, full


def node(mat, cls, x, y):
    return mel.create_material_expression(mat, cls, x, y)


def link(a, ao, b, bi, label):
    if not mel.connect_material_expressions(a, ao, b, bi):
        fails.append(label)


def scalar(mat, name, default, x, y):
    n = node(mat, unreal.MaterialExpressionScalarParameter, x, y)
    n.set_editor_property('parameter_name', name)
    n.set_editor_property('default_value', default)
    return n


def vector(mat, name, r, g, b, x, y):
    n = node(mat, unreal.MaterialExpressionVectorParameter, x, y)
    n.set_editor_property('parameter_name', name)
    n.set_editor_property('default_value', unreal.LinearColor(r, g, b, 1.0))
    return n


def mask(mat, src, out, x, y, rgb, label):
    m = node(mat, unreal.MaterialExpressionComponentMask, x, y)
    m.set_editor_property('r', True)
    m.set_editor_property('g', rgb)
    m.set_editor_property('b', rgb)
    m.set_editor_property('a', False)
    link(src, out, m, '', label)
    return m


def saturate(mat, src, x, y, label):
    s = node(mat, unreal.MaterialExpressionSaturate, x, y)
    link(src, '', s, '', label)
    return s


def finish(mat, full):
    mel.recompile_material(mat)
    unreal.EditorAssetLibrary.save_asset(full)


# ---------------------------------------------------------------------------------------------
# M_FXR_Outline
#
# The stencil carries BOTH the highlight state and how far it has faded: State + Level * 4, with
# state 1 Hover / 2 Guidance / 3 Selected and level 0..63. One full-screen pass can read nothing
# else per object, and it needs both — which colour, and how strong.
# ---------------------------------------------------------------------------------------------
def build_outline():
    mat, full = new_material('M_FXR_Outline', domain=unreal.MaterialDomain.MD_POST_PROCESS)

    thickness = scalar(mat, 'OutlineThickness', 2.0, -3200, 500)

    centre_tex = node(mat, unreal.MaterialExpressionSceneTexture, -2400, -700)
    centre_tex.set_editor_property('scene_texture_id', unreal.SceneTextureId.PPI_CUSTOM_STENCIL)
    screen_uv = node(mat, unreal.MaterialExpressionScreenPosition, -3200, -900)

    def neighbour(ox, oy, y, tag):
        tex = node(mat, unreal.MaterialExpressionSceneTexture, -1700, y)
        tex.set_editor_property('scene_texture_id', unreal.SceneTextureId.PPI_CUSTOM_STENCIL)

        px = node(mat, unreal.MaterialExpressionConstant2Vector, -2900, y + 140)
        px.set_editor_property('r', ox)
        px.set_editor_property('g', oy)

        scaled = node(mat, unreal.MaterialExpressionMultiply, -2700, y + 140)
        link(px, '', scaled, 'A', '%s px' % tag)
        link(thickness, '', scaled, 'B', '%s thick' % tag)

        uvdelta = node(mat, unreal.MaterialExpressionMultiply, -2400, y + 140)
        link(scaled, '', uvdelta, 'A', '%s scaled' % tag)
        link(centre_tex, 'InvSize', uvdelta, 'B', '%s invsize' % tag)

        uv = node(mat, unreal.MaterialExpressionAdd, -2100, y + 60)
        link(screen_uv, 'ViewportUV', uv, 'A', '%s uv.A' % tag)
        link(uvdelta, '', uv, 'B', '%s uv.B' % tag)
        link(uv, '', tex, 'UVs', '%s uv->tex' % tag)

        return mask(mat, tex, 'Color', -1450, y, False, '%s mask' % tag)

    centre = mask(mat, centre_tex, 'Color', -2150, -700, False, 'centre mask')
    neigh = [
        neighbour(1.0, 0.0, -300, 'nR'),
        neighbour(-1.0, 0.0, 100, 'nL'),
        neighbour(0.0, 1.0, 500, 'nD'),
        neighbour(0.0, -1.0, 900, 'nU'),
    ]

    # Highest raw stencil among the neighbours. Because the fade level occupies the high bits, this
    # picks the most faded-in neighbour, and unpacking that one value keeps state and strength paired.
    peak = neigh[0]
    for i, n in enumerate(neigh[1:]):
        m = node(mat, unreal.MaterialExpressionMax, -1200, -100 + i * 300)
        link(peak, '', m, 'A', 'peak max%d' % i)
        link(n, '', m, 'B', 'n%d max' % i)
        peak = m

    # Unpack: level = floor(raw / 4), state = raw - level * 4, alpha = level / 63.
    quarter = node(mat, unreal.MaterialExpressionMultiply, -1050, 700)
    quarter.set_editor_property('const_b', 0.25)
    link(peak, '', quarter, 'A', 'peak->quarter')

    level = node(mat, unreal.MaterialExpressionFloor, -900, 700)
    link(quarter, '', level, '', 'quarter->floor')

    level4 = node(mat, unreal.MaterialExpressionMultiply, -750, 780)
    level4.set_editor_property('const_b', 4.0)
    link(level, '', level4, 'A', 'level->level4')

    state = node(mat, unreal.MaterialExpressionSubtract, -600, 700)
    link(peak, '', state, 'A', 'peak->state.A')
    link(level4, '', state, 'B', 'level4->state.B')

    fade = node(mat, unreal.MaterialExpressionMultiply, -750, 900)
    fade.set_editor_property('const_b', 1.0 / 63.0)
    link(level, '', fade, 'A', 'level->fade')

    # Band = a lit neighbour with an unlit centre, so the outline sits just outside the silhouette.
    lit_near = saturate(mat, state, -450, 500, 'state->sat')
    lit_here = saturate(mat, centre, -1000, -700, 'centre->sat')
    outside = node(mat, unreal.MaterialExpressionSubtract, -850, -700)
    outside.set_editor_property('const_a', 1.0)
    link(lit_here, '', outside, 'B', 'lit_here->outside')

    band = node(mat, unreal.MaterialExpressionMultiply, -300, -400)
    link(lit_near, '', band, 'A', 'lit_near->band')
    link(outside, '', band, 'B', 'outside->band')

    # Faded in and out rather than switched, so a hover never pops on or cuts off.
    alpha = node(mat, unreal.MaterialExpressionMultiply, -150, -300)
    link(band, '', alpha, 'A', 'band->alpha')
    link(fade, '', alpha, 'B', 'fade->alpha')

    hover = mask(mat, vector(mat, 'HoverColor', 1.0, 1.0, 1.0, -3200, -1900), '', -2950, -1900, True, 'hover mask')
    guidance = mask(mat, vector(mat, 'GuidanceColor', 1.0, 0.75, 0.15, -3200, -1700), '', -2950, -1700, True, 'guidance mask')
    selected = mask(mat, vector(mat, 'SelectedColor', 0.2, 1.0, 0.4, -3200, -1500), '', -2950, -1500, True, 'selected mask')

    step1 = node(mat, unreal.MaterialExpressionSubtract, -1000, -1300)
    step1.set_editor_property('const_b', 1.0)
    link(state, '', step1, 'A', 'state->step1')
    t1 = saturate(mat, step1, -850, -1300, 'step1->sat')

    step2 = node(mat, unreal.MaterialExpressionSubtract, -1000, -1150)
    step2.set_editor_property('const_b', 2.0)
    link(state, '', step2, 'A', 'state->step2')
    t2 = saturate(mat, step2, -850, -1150, 'step2->sat')

    pick1 = node(mat, unreal.MaterialExpressionLinearInterpolate, -600, -1750)
    link(hover, '', pick1, 'A', 'hover->pick1')
    link(guidance, '', pick1, 'B', 'guidance->pick1')
    link(t1, '', pick1, 'Alpha', 't1->pick1')

    pick2 = node(mat, unreal.MaterialExpressionLinearInterpolate, -420, -1650)
    link(pick1, '', pick2, 'A', 'pick1->pick2')
    link(selected, '', pick2, 'B', 'selected->pick2')
    link(t2, '', pick2, 'Alpha', 't2->pick2')

    intensity = scalar(mat, 'OutlineIntensity', 3.0, -600, -1450)
    glow = node(mat, unreal.MaterialExpressionMultiply, -250, -1600)
    link(pick2, '', glow, 'A', 'pick2->glow')
    link(intensity, '', glow, 'B', 'intensity->glow')

    scene = node(mat, unreal.MaterialExpressionSceneTexture, -900, -2200)
    scene.set_editor_property('scene_texture_id', unreal.SceneTextureId.PPI_POST_PROCESS_INPUT0)
    scene_rgb = mask(mat, scene, 'Color', -600, -2200, True, 'scene mask')

    out = node(mat, unreal.MaterialExpressionLinearInterpolate, 0, -1000)
    link(scene_rgb, '', out, 'A', 'scene->out')
    link(glow, '', out, 'B', 'glow->out')
    link(alpha, '', out, 'Alpha', 'alpha->out')

    if not mel.connect_material_property(out, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR):
        fails.append('outline->EmissiveColor')
    finish(mat, full)


# ---------------------------------------------------------------------------------------------
# M_FXR_HighlightOverlay — Inner Blink and Sweep, one material chosen by a scalar (a dynamic
# instance can set scalars at runtime but cannot flip static switches).
# ---------------------------------------------------------------------------------------------
def build_overlay():
    mat, full = new_material('M_FXR_HighlightOverlay',
                             blend=unreal.BlendMode.BLEND_TRANSLUCENT,
                             shading=unreal.MaterialShadingModel.MSM_UNLIT)

    colour = vector(mat, 'HighlightColor', 1.0, 0.75, 0.15, -1800, -900)
    colour_rgb = mask(mat, colour, '', -1550, -900, True, 'colour mask')
    intensity = scalar(mat, 'HighlightIntensity', 1.0, -1800, -740)
    pulse_rate = scalar(mat, 'PulseRate', 1.5, -1800, -560)
    sweep_amount = scalar(mat, 'SweepAmount', 0.0, -1800, -420)
    fade_alpha = scalar(mat, 'FadeAlpha', 1.0, -1800, 900)
    sweep_dir = vector(mat, 'SweepDirection', 0.0, 0.0, 1.0, -1800, 500)

    emissive = node(mat, unreal.MaterialExpressionMultiply, -1300, -820)
    link(colour_rgb, '', emissive, 'A', 'colour->emissive')
    link(intensity, '', emissive, 'B', 'intensity->emissive')

    time = node(mat, unreal.MaterialExpressionTime, -1800, -200)
    phase = node(mat, unreal.MaterialExpressionMultiply, -1550, -200)
    link(time, '', phase, 'A', 'time->phase')
    link(pulse_rate, '', phase, 'B', 'rate->phase')

    sine = node(mat, unreal.MaterialExpressionSine, -1350, -260)
    link(phase, '', sine, '', 'phase->sine')
    half = node(mat, unreal.MaterialExpressionMultiply, -1180, -260)
    half.set_editor_property('const_b', 0.5)
    link(sine, '', half, 'A', 'sine->half')
    blink = node(mat, unreal.MaterialExpressionAdd, -1020, -260)
    blink.set_editor_property('const_b', 0.5)
    link(half, '', blink, 'A', 'half->blink')

    world_pos = node(mat, unreal.MaterialExpressionWorldPosition, -1800, 200)
    obj_pos = node(mat, unreal.MaterialExpressionObjectPositionWS, -1800, 340)
    delta = node(mat, unreal.MaterialExpressionSubtract, -1550, 240)
    link(world_pos, '', delta, 'A', 'worldpos->delta')
    link(obj_pos, '', delta, 'B', 'objpos->delta')

    dir_n = node(mat, unreal.MaterialExpressionNormalize, -1550, 500)
    link(sweep_dir, '', dir_n, '', 'sweepdir->normalize')

    coord = node(mat, unreal.MaterialExpressionDotProduct, -1350, 300)
    link(delta, '', coord, 'A', 'delta->dot')
    link(dir_n, '', coord, 'B', 'dir->dot')

    radius = node(mat, unreal.MaterialExpressionObjectRadius, -1550, 650)
    norm = node(mat, unreal.MaterialExpressionDivide, -1180, 350)
    link(coord, '', norm, 'A', 'dot->norm')
    link(radius, '', norm, 'B', 'radius->norm')

    lead = node(mat, unreal.MaterialExpressionMultiply, -1020, 350)
    lead.set_editor_property('const_b', 0.5)
    link(norm, '', lead, 'A', 'norm->lead')

    travel = node(mat, unreal.MaterialExpressionSubtract, -870, 250)
    link(phase, '', travel, 'A', 'phase->travel')
    link(lead, '', travel, 'B', 'lead->travel')

    wrapped = node(mat, unreal.MaterialExpressionFrac, -720, 250)
    link(travel, '', wrapped, '', 'travel->frac')

    centred = node(mat, unreal.MaterialExpressionMultiply, -570, 250)
    centred.set_editor_property('const_b', 2.0)
    link(wrapped, '', centred, 'A', 'frac->centred')
    shifted = node(mat, unreal.MaterialExpressionSubtract, -430, 250)
    shifted.set_editor_property('const_b', 1.0)
    link(centred, '', shifted, 'A', 'centred->shifted')
    dist = node(mat, unreal.MaterialExpressionAbs, -290, 250)
    link(shifted, '', dist, '', 'shifted->abs')
    sharp = node(mat, unreal.MaterialExpressionMultiply, -150, 250)
    sharp.set_editor_property('const_b', 3.0)
    link(dist, '', sharp, 'A', 'abs->sharp')
    inv = node(mat, unreal.MaterialExpressionSubtract, -10, 250)
    inv.set_editor_property('const_a', 1.0)
    link(sharp, '', inv, 'B', 'sharp->inv')
    band = node(mat, unreal.MaterialExpressionSaturate, 130, 250)
    link(inv, '', band, '', 'inv->saturate')

    style = node(mat, unreal.MaterialExpressionLinearInterpolate, 320, 0)
    link(blink, '', style, 'A', 'blink->style')
    link(band, '', style, 'B', 'band->style')
    link(sweep_amount, '', style, 'Alpha', 'sweep->style')

    # Capped below 1: at full opacity the pulse replaces the mesh outright, which reads as a glitch
    # rather than a glow over the object. Brightness is HighlightIntensity's job.
    capped = node(mat, unreal.MaterialExpressionMultiply, 480, 0)
    capped.set_editor_property('const_b', 0.65)
    link(style, '', capped, 'A', 'style->capped')

    # Faded in and out with the highlight, so Inner Blink and Sweep never pop on.
    opacity = node(mat, unreal.MaterialExpressionMultiply, 640, 100)
    link(capped, '', opacity, 'A', 'capped->opacity')
    link(fade_alpha, '', opacity, 'B', 'fade->opacity')

    if not mel.connect_material_property(emissive, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR):
        fails.append('overlay->EmissiveColor')
    if not mel.connect_material_property(opacity, '', unreal.MaterialProperty.MP_OPACITY):
        fails.append('overlay->Opacity')
    finish(mat, full)


# ---------------------------------------------------------------------------------------------
# M_FXR_Ghost — FXR_Socket's placement preview.
# ---------------------------------------------------------------------------------------------
def build_ghost():
    mat, full = new_material('M_FXR_Ghost',
                             blend=unreal.BlendMode.BLEND_TRANSLUCENT,
                             shading=unreal.MaterialShadingModel.MSM_UNLIT,
                             two_sided=True)

    colour = vector(mat, 'GhostColor', 0.35, 0.75, 1.0, -900, -300)
    rgb = mask(mat, colour, '', -650, -300, True, 'ghost colour mask')
    intensity = scalar(mat, 'GhostIntensity', 1.5, -900, -140)

    emissive = node(mat, unreal.MaterialExpressionMultiply, -420, -260)
    link(rgb, '', emissive, 'A', 'rgb->emissive')
    link(intensity, '', emissive, 'B', 'intensity->emissive')

    # Fresnel so the silhouette reads strongest: a flat translucent block looks like a bug, while an
    # object whose edges glow reads as a placement hint.
    fres = node(mat, unreal.MaterialExpressionFresnel, -900, 120)
    fres.set_editor_property('exponent', 2.0)
    fres.set_editor_property('base_reflect_fraction', 0.35)

    base = node(mat, unreal.MaterialExpressionMultiply, -600, 120)
    base.set_editor_property('const_b', 0.55)
    link(fres, '', base, 'A', 'fresnel->base')

    # Driven by the socket so the preview fades in and out instead of blinking on.
    fade = scalar(mat, 'GhostOpacity', 1.0, -900, 300)
    opacity = node(mat, unreal.MaterialExpressionMultiply, -420, 200)
    link(base, '', opacity, 'A', 'base->opacity')
    link(fade, '', opacity, 'B', 'fade->opacity')

    if not mel.connect_material_property(emissive, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR):
        fails.append('ghost->EmissiveColor')
    if not mel.connect_material_property(opacity, '', unreal.MaterialProperty.MP_OPACITY):
        fails.append('ghost->Opacity')
    finish(mat, full)


# ---------------------------------------------------------------------------------------------
# M_FXR_Ray — the far-interaction pointer beam, drawn on the arc-segment tube.
# ---------------------------------------------------------------------------------------------
def build_ray():
    # Opaque, not translucent and not additive. Both of those compiled cleanly, reported no errors and
    # rendered nothing at all on this mesh, while a plain unlit opaque material on the very same
    # component drew immediately — and pushing emissive to 60 did not bring the additive version back,
    # so it was never exposure. A pointer reads as a solid emissive tube in every shipping VR title
    # anyway, so this drops the fragile path rather than keeping a fade the renderer will not draw.
    # The fade lives in the geometry instead: the driver thins the beam to nothing.
    mat, full = new_material('M_FXR_Ray',
                             blend=unreal.BlendMode.BLEND_OPAQUE,
                             shading=unreal.MaterialShadingModel.MSM_UNLIT,
                             two_sided=True)

    colour = vector(mat, 'RayColor', 0.45, 0.8, 1.0, -900, -300)
    rgb = mask(mat, colour, '', -650, -300, True, 'ray colour mask')
    # 1.0, not 2. Unlit emissive above 1 clips after tonemapping, and the beam came out white
    # instead of cyan — the same trap the highlight overlay hit. Intensity here is for pushing a
    # beam through a bright scene deliberately, not a default.
    intensity = scalar(mat, 'RayIntensity', 1.0, -900, -140)

    emissive = node(mat, unreal.MaterialExpressionMultiply, -420, -260)
    link(rgb, '', emissive, 'A', 'rgb->emissive')
    link(intensity, '', emissive, 'B', 'intensity->emissive')

    if not mel.connect_material_property(emissive, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR):
        fails.append('ray->EmissiveColor')
    finish(mat, full)


# Each is independent, so one failure still leaves the others rebuilt and reports what broke
# rather than stopping halfway with no explanation.
for label, builder in (('M_FXR_Outline', build_outline),
                       ('M_FXR_HighlightOverlay', build_overlay),
                       ('M_FXR_Ghost', build_ghost),
                       ('M_FXR_Ray', build_ray)):
    try:
        builder()
        print('FXR_MATERIAL %s: rebuilt' % label)
    except Exception as error:
        fails.append('%s raised %s' % (label, error))

print('FXR_MATERIALS: %s' % ('OK' if not fails else 'FAILED %s' % fails))
