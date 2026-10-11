// Copyright (c) 2026 Low Sze Hao. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/Font.h"
#include "Styling/SlateBrush.h"
#include "Styling/SlateColor.h"
#include "Styling/SlateTypes.h"

/**
 * The training panel's design system, in one place.
 *
 * Both halves of the panel read from here: the editor builder that lays out the Widget Blueprint,
 * and the runtime panel that builds the step rows as the procedure runs. A token defined twice is a
 * token that will disagree with itself, and a row that does not match the list it sits in is the
 * most visible kind of wrong.
 *
 *
 * THE ONE NUMBER EVERYTHING ELSE COMES FROM
 *
 *     Draw Size 900 x 870 px at Panel Width 90 cm  ->  component scale 0.1  ->  1 px = 1 mm
 *
 * That is why the sizes below are bare numbers and still mean something physical: 56 is 5.6 cm of
 * cap height, and at a 1.5 m reading distance that is about 37 dmm, which sits in the middle of the
 * band reading studies on this hardware settle on. Change the 90 cm and every size below silently
 * becomes a different angular size - so change the panel's distance instead, which is the knob that
 * was always meant to be turned.
 *
 * The companion page is `UI_Design/panel.html`, drawn at these exact numbers. It is the spec: if
 * this file and that page disagree, the page is right.
 *
 *
 * WHAT IS DELIBERATELY NOT REPRODUCED
 *
 * The design's surface is frosted glass - `backdrop-filter: blur(30px)`. A world-space
 * `UWidgetComponent` cannot do that: there is no scene colour behind the quad for the widget to
 * read, so there is nothing to blur. What survives is the part that carries the look anyway, a
 * translucent near-black fill with a bright inner ring, and that is what Surface() and PanelRing()
 * are. Nothing here is waiting on an engine feature; the blur is simply not available and saying so
 * is cheaper than someone looking for the bug.
 */
namespace FXR_PanelStyle
{
	// ── colour ──────────────────────────────────────────────────────────────────────────────────
	//
	// Every value in the design is sRGB; FLinearColor is linear, and the conversion is a curve
	// rather than a divide. Pasting hex straight in is what makes a panel come out washed out and
	// too bright, so the hex stays readable here and the conversion happens once.

	/** Hex the way a colour picker shows it, with alpha kept linear-independent. */
	inline FLinearColor Srgb(uint8 R, uint8 G, uint8 B, float A = 1.f)
	{
		FLinearColor Out = FLinearColor::FromSRGBColor(FColor(R, G, B));
		Out.A = A;
		return Out;
	}

	/** The panel itself. Near-black and translucent, never opaque: it is a thing in a room. */
	inline FLinearColor Surface()     { return Srgb(28, 28, 30, 0.78f); }

	/** The inner ring that gives the surface an edge, standing in for the glass highlight. */
	inline FLinearColor PanelRing()   { return Srgb(255, 255, 255, 0.13f); }

	inline FLinearColor Text()        { return Srgb(255, 255, 255); }
	inline FLinearColor Text2()       { return Srgb(235, 235, 245, 0.68f); }
	inline FLinearColor Text3()       { return Srgb(235, 235, 245, 0.46f); }
	inline FLinearColor Text4()       { return Srgb(235, 235, 245, 0.26f); }

	/** Dividers, and the ring around anything that is not the panel. */
	inline FLinearColor Divider()     { return Srgb(255, 255, 255, 0.09f); }

	/** A neutral control face. Fill2 is the ground a list sits on; Track is a bar's empty part. */
	inline FLinearColor Fill()        { return Srgb(120, 120, 128, 0.20f); }
	inline FLinearColor Fill2()       { return Srgb(120, 120, 128, 0.13f); }
	inline FLinearColor Track()       { return Srgb(120, 120, 128, 0.26f); }

	/** The one accent. Progress, the open step, and the single affirmative button. */
	inline FLinearColor Blue()        { return Srgb(10, 132, 255); }
	inline FLinearColor OnBlue()      { return Srgb(4, 34, 61); }

	inline FLinearColor Green()       { return Srgb(48, 209, 88); }
	inline FLinearColor OnGreen()     { return Srgb(5, 38, 26); }

	/** Red is the event; Danger is red made readable as text. A pure red headline is unreadable. */
	inline FLinearColor Red()         { return Srgb(255, 69, 58); }
	inline FLinearColor Danger()      { return Srgb(255, 138, 128); }

	inline FLinearColor Amber()       { return Srgb(255, 159, 10); }

	/** The explanation's own colour, so a briefing never looks like an instruction. */
	inline FLinearColor Explain()     { return Srgb(255, 207, 112); }

	/** The bed a mistake sits on, and the brighter rule above it. */
	inline FLinearColor AlarmBed()    { return Srgb(255, 69, 58, 0.13f); }
	inline FLinearColor AlarmRule()   { return Srgb(255, 69, 58, 0.40f); }

	/** The open step's row. Tinted rather than outlined, so the list keeps one edge. */
	inline FLinearColor ActiveRow()   { return Srgb(10, 132, 255, 0.10f); }

	// ── metrics ─────────────────────────────────────────────────────────────────────────────────
	//
	// Millimetres, by the identity at the top of this file.

	constexpr float PanelW        = 900.f;
	constexpr float PanelH        = 870.f;
	constexpr float PanelRadius   = 42.f;
	constexpr float PanelRingW    = 2.f;

	/** The horizontal margin every screen shares. Content that breaks it reads as a mistake. */
	constexpr float Gutter        = 52.f;

	constexpr float BarH          = 104.f;
	constexpr float FootH         = 134.f;
	constexpr float ButtonH       = 122.f;

	/**
	 * Dividers are 3 px bands, not 1 px hairlines.
	 *
	 * There is no temporal AA on this target and MSAA does nothing for content drawn inside a
	 * widget, so a one-pixel rule crawls with every small head movement. Three pixels is the
	 * thinnest line that holds still.
	 */
	constexpr float RuleW         = 3.f;

	/** One segment per step, riding directly under the bar instead of costing a whole block. */
	constexpr float MeterH        = 6.f;
	constexpr float MeterGap      = 3.f;

	constexpr float ListRadius    = 22.f;
	constexpr float RowH          = 60.f;
	constexpr float RowActiveH    = 96.f;
	constexpr float RowPadX        = 18.f;
	constexpr float RowPadY        = 10.f;
	constexpr float NumeralSize   = 42.f;

	/** The score ring, and the width of its band. */
	constexpr float RingSize      = 200.f;
	constexpr float RingThickness = 12.f;

	// ── type ────────────────────────────────────────────────────────────────────────────────────
	//
	// Montserrat, three weights. The design asks for 500, 600 and 800 and uses no italics, so
	// those three faces are all that is imported - an unused face is still a cooked megabyte.

	/** Typeface names inside the composite font. These are the strings the UFont was built with. */
	inline const FName FontMedium()    { return TEXT("Medium"); }      // 500
	inline const FName FontSemiBold()  { return TEXT("SemiBold"); }    // 600
	inline const FName FontExtraBold() { return TEXT("ExtraBold"); }   // 800

	/**
	 * The font asset.
	 *
	 * Soft-loaded on purpose. A hard reference from C++ roots the asset, which is what takes the
	 * editor down when a tool tries to replace it - the same trap the material regen script fell
	 * into. The cost is that nothing records a dependency edge for the cooker, so `/FlexXR/UI` is
	 * named in DirectoriesToAlwaysCook; the Fonts folder sits under it and is covered.
	 */
	inline UFont* Font()
	{
		static const FSoftObjectPath Path(TEXT("/FlexXR/UI/Fonts/F_Montserrat.F_Montserrat"));

		// Weak, not strong. Holding a hard pointer here would root the asset, which is the trap the
		// material tools fell into - a rooted asset cannot be replaced, and the tool that tries
		// takes the editor with it. Weak also means this survives the asset being reimported.
		static TWeakObjectPtr<UFont> Cached;

		if (!Cached.IsValid())
		{
			Cached = Cast<UFont>(Path.TryLoad());
		}

		return Cached.Get();
	}

	/**
	 * A font at a size and a weight.
	 *
	 * Letter spacing is in thousandths of an em, so the design's 0.14em is 140 and its -0.028em is
	 * -28. Negative tracking on a large heading is not a flourish: at 56 px the default gaps read
	 * as loose, and closing them is what makes a title look set rather than typed.
	 */
	inline FSlateFontInfo Type(int32 Size, FName Weight, int32 Tracking = 0)
	{
		FSlateFontInfo Info(Font(), Size, Weight);
		Info.LetterSpacing = Tracking;
		return Info;
	}

	//~ The type scale. Named for the job, not the size, so a change lands everywhere it should.
	inline FSlateFontInfo TypeEyebrow()    { return Type(20, FontExtraBold(), 140); }
	inline FSlateFontInfo TypeTitle()      { return Type(56, FontExtraBold(), -28); }
	inline FSlateFontInfo TypeHeading()    { return Type(40, FontExtraBold(), -24); }
	inline FSlateFontInfo TypeBody()       { return Type(40, FontMedium()); }
	inline FSlateFontInfo TypeReason()     { return Type(24, FontMedium()); }
	inline FSlateFontInfo TypeCrumb()      { return Type(24, FontSemiBold()); }
	inline FSlateFontInfo TypeStatus()     { return Type(22, FontMedium()); }
	inline FSlateFontInfo TypeKind()       { return Type(24, FontSemiBold(), 140); }
	inline FSlateFontInfo TypeButton()     { return Type(34, FontSemiBold(), -10); }
	inline FSlateFontInfo TypeRow()        { return Type(21, FontMedium()); }
	inline FSlateFontInfo TypeRowActive()  { return Type(24, FontSemiBold()); }
	inline FSlateFontInfo TypeNumeral()    { return Type(20, FontSemiBold()); }
	inline FSlateFontInfo TypeDefTerm()    { return Type(24, FontMedium()); }
	inline FSlateFontInfo TypeDefValue()   { return Type(24, FontSemiBold()); }
	inline FSlateFontInfo TypeScore()      { return Type(64, FontExtraBold(), -30); }
	inline FSlateFontInfo TypeMistake()    { return Type(30, FontSemiBold()); }
	inline FSlateFontInfo TypeTime()       { return Type(26, FontMedium()); }

	/** Line heights, as the multiplier UTextBlock wants. */
	constexpr float LineBody      = 1.52f;
	constexpr float LineRow       = 1.30f;
	constexpr float LineRowActive = 1.26f;
	constexpr float LineReason    = 1.40f;
	constexpr float LineHeading   = 1.10f;
	constexpr float LineTitle     = 1.06f;

	// ── brushes ─────────────────────────────────────────────────────────────────────────────────
	//
	// Rounded Box with no image, which is the whole reason this design needs no nine-slice
	// textures. Corner Radii is an FVector4 of (TL, TR, BR, BL) when per-corner is wanted, and a
	// pill is Half Height Radius rather than a number anyone has to keep in sync with a height.

	/** A filled rounded rectangle. */
	inline FSlateBrush Box(const FLinearColor& Colour, float Radius)
	{
		FSlateBrush Brush;
		Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
		Brush.TintColor = FSlateColor(Colour);
		Brush.OutlineSettings.CornerRadii = FVector4(Radius, Radius, Radius, Radius);
		Brush.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
		return Brush;
	}

	/** The same, with an inner stroke. Slate's outline is always inside the shape. */
	inline FSlateBrush RingedBox(const FLinearColor& Colour, float Radius,
		const FLinearColor& Ring, float RingWidth)
	{
		FSlateBrush Brush = Box(Colour, Radius);
		Brush.OutlineSettings.Color = FSlateColor(Ring);
		Brush.OutlineSettings.Width = RingWidth;
		return Brush;
	}

	/** A pill, whose radius follows its own height so nothing has to be recomputed. */
	inline FSlateBrush Pill(const FLinearColor& Colour)
	{
		FSlateBrush Brush = Box(Colour, 0.f);
		Brush.OutlineSettings.RoundingType = ESlateBrushRoundingType::HalfHeightRadius;
		return Brush;
	}

	inline FSlateBrush RingedPill(const FLinearColor& Colour, const FLinearColor& Ring, float RingWidth)
	{
		FSlateBrush Brush = Pill(Colour);
		Brush.OutlineSettings.Color = FSlateColor(Ring);
		Brush.OutlineSettings.Width = RingWidth;
		return Brush;
	}

	/** A dot. A pill of equal sides is a circle, so this needs no circle texture either. */
	inline FSlateBrush Dot(const FLinearColor& Colour, float Diameter)
	{
		FSlateBrush Brush = Pill(Colour);
		Brush.ImageSize = FVector2D(Diameter, Diameter);
		return Brush;
	}

	/** A flat band, for dividers and meter segments. */
	inline FSlateBrush Band(const FLinearColor& Colour)
	{
		FSlateBrush Brush;
		Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
		Brush.TintColor = FSlateColor(Colour);
		Brush.OutlineSettings.CornerRadii = FVector4(0.f, 0.f, 0.f, 0.f);
		Brush.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
		return Brush;
	}

	/** A thin pill track with a pill fill, which is every bar on the panel. */
	inline FProgressBarStyle BarStyle(const FLinearColor& TrackColour, const FLinearColor& FillColour)
	{
		FProgressBarStyle Style;
		Style.BackgroundImage = Pill(TrackColour);
		Style.FillImage = Pill(FillColour);
		Style.EnableFillAnimation = false;
		return Style;
	}

	/**
	 * A button face.
	 *
	 * Hovered and pressed are brightened rather than recoloured, because a laser hover on a world
	 * panel has no cursor to explain itself: the only thing that says "this one" is the face
	 * changing, and a hue change would read as a different kind of button instead.
	 */
	inline FButtonStyle ButtonFace(const FLinearColor& Face, const FLinearColor& Ring, float RingWidth)
	{
		const FLinearColor Hover(Face.R * 1.18f, Face.G * 1.18f, Face.B * 1.18f, FMath::Min(1.f, Face.A + 0.06f));
		const FLinearColor Press(Face.R * 0.86f, Face.G * 0.86f, Face.B * 0.86f, Face.A);

		FButtonStyle Style;
		Style.SetNormal(RingWidth > 0.f ? RingedPill(Face, Ring, RingWidth) : Pill(Face));
		Style.SetHovered(RingWidth > 0.f ? RingedPill(Hover, Ring, RingWidth) : Pill(Hover));
		Style.SetPressed(RingWidth > 0.f ? RingedPill(Press, Ring, RingWidth) : Pill(Press));
		Style.SetDisabled(Pill(Fill2()));
		Style.SetNormalPadding(FMargin(0.f));
		Style.SetPressedPadding(FMargin(0.f));
		return Style;
	}

	/** Seconds as m:ss. Every clock on the panel, so they cannot drift apart in format. */
	inline FText Clock(float Seconds)
	{
		const int32 Whole = FMath::Max(0, FMath::FloorToInt32(Seconds));
		return FText::FromString(FString::Printf(TEXT("%d:%02d"), Whole / 60, Whole % 60));
	}
}
