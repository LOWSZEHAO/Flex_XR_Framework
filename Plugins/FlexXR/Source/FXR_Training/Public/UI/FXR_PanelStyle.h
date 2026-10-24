// Copyright (c) 2026 Low Sze Hao. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/Font.h"
#include "Styling/SlateBrush.h"
#include "Styling/SlateColor.h"
#include "Styling/SlateTypes.h"

/**
 * Design tokens for the training panel: colours, sizes, fonts and brushes.
 *
 * Shared by the editor builder (which lays out the Widget Blueprint) and the runtime panel (which
 * builds the rows during a run), so both always match.
 *
 * Scale: Draw Size 900 x 870 at Panel Width 90 cm gives component scale 0.1, so 1 px = 1 mm. The
 * sizes below are in those units. To make text bigger or smaller, move the panel closer or further
 * rather than changing the 90 cm.
 *
 * The reference mockup is UI_Design/panel.html, drawn at the same numbers.
 *
 * The mockup uses a frosted-glass blur, which a world-space widget can't do (there's no scene
 * colour behind it to blur), so the panel uses a translucent dark fill with a light inner ring.
 */
namespace FXR_PanelStyle
{
	// -- colour ----------------------------------------------------------------------------------
	//
	// The design values are sRGB hex; FLinearColor is linear. Convert once here, otherwise the panel
	// comes out washed out.

	/** sRGB bytes to linear. Alpha is passed through unchanged. */
	inline FLinearColor Srgb(uint8 R, uint8 G, uint8 B, float A = 1.f)
	{
		FLinearColor Out = FLinearColor::FromSRGBColor(FColor(R, G, B));
		Out.A = A;
		return Out;
	}

	/** Panel background. Dark and translucent. */
	inline FLinearColor Surface()     { return Srgb(28, 28, 30, 0.78f); }

	/** Inner ring around the panel (stands in for the glass edge). */
	inline FLinearColor PanelRing()   { return Srgb(255, 255, 255, 0.13f); }

	inline FLinearColor Text()        { return Srgb(255, 255, 255); }
	inline FLinearColor Text2()       { return Srgb(235, 235, 245, 0.68f); }
	inline FLinearColor Text3()       { return Srgb(235, 235, 245, 0.46f); }
	inline FLinearColor Text4()       { return Srgb(235, 235, 245, 0.26f); }

	/** Dividers, and rings around non-panel elements. */
	inline FLinearColor Divider()     { return Srgb(255, 255, 255, 0.09f); }

	/** Neutral fills. Fill2 is the list background, Track is the empty part of a bar. */
	inline FLinearColor Fill()        { return Srgb(120, 120, 128, 0.20f); }
	inline FLinearColor Fill2()       { return Srgb(120, 120, 128, 0.13f); }
	inline FLinearColor Track()       { return Srgb(120, 120, 128, 0.26f); }

	/** The accent colour: progress, the open step, the main button. */
	inline FLinearColor Blue()        { return Srgb(10, 132, 255); }
	inline FLinearColor OnBlue()      { return Srgb(4, 34, 61); }

	inline FLinearColor Green()       { return Srgb(48, 209, 88); }
	inline FLinearColor OnGreen()     { return Srgb(5, 38, 26); }

	/** Red for failure. Danger is a lighter red for text, since pure red text is hard to read. */
	inline FLinearColor Red()         { return Srgb(255, 69, 58); }
	inline FLinearColor Danger()      { return Srgb(255, 138, 128); }

	inline FLinearColor Amber()       { return Srgb(255, 159, 10); }

	/** Briefing colour, so it doesn't look like an instruction. */
	inline FLinearColor Explain()     { return Srgb(255, 207, 112); }

	/** Mistake message background and its top rule. */
	inline FLinearColor AlarmBed()    { return Srgb(255, 69, 58, 0.13f); }
	inline FLinearColor AlarmRule()   { return Srgb(255, 69, 58, 0.40f); }

	/** Open row tint. */
	inline FLinearColor ActiveRow()   { return Srgb(10, 132, 255, 0.10f); }

	// -- metrics ---------------------------------------------------------------------------------
	//
	// In panel pixels (= mm, see above).

	constexpr float PanelW        = 900.f;
	constexpr float PanelH        = 870.f;
	constexpr float PanelRadius   = 42.f;
	constexpr float PanelRingW    = 2.f;

	/** Horizontal margin shared by every screen. */
	constexpr float Gutter        = 52.f;

	constexpr float BarH          = 104.f;
	constexpr float FootH         = 134.f;
	constexpr float ButtonH       = 122.f;

	/**
	 * Dividers are 3 px instead of 1 px. Without temporal AA a 1 px line shimmers with head
	 * movement; 3 px is the thinnest that stays stable.
	 */
	constexpr float RuleW         = 3.f;

	/** Progress meter under the top bar: one segment per step. */
	constexpr float MeterH        = 6.f;
	constexpr float MeterGap      = 3.f;

	constexpr float ListRadius    = 22.f;
	constexpr float RowH          = 60.f;
	constexpr float RowActiveH    = 96.f;
	constexpr float RowPadX        = 18.f;
	constexpr float RowPadY        = 10.f;
	constexpr float NumeralSize   = 42.f;

	/** Score ring size and band width. */
	constexpr float RingSize      = 200.f;
	constexpr float RingThickness = 12.f;

	// -- type ------------------------------------------------------------------------------------
	//
	// Montserrat in three weights (500, 600, 800). Only those three are imported to keep the cooked
	// size down.

	/** Typeface names inside the composite font. */
	inline const FName FontMedium()    { return TEXT("Medium"); }      // 500
	inline const FName FontSemiBold()  { return TEXT("SemiBold"); }    // 600
	inline const FName FontExtraBold() { return TEXT("ExtraBold"); }   // 800

	/**
	 * The font asset, soft-loaded. A hard reference from C++ roots the asset, and then the editor
	 * crashes when a tool tries to replace it. Since there's no hard reference for the cooker to
	 * follow, /FlexXR/UI is listed in DirectoriesToAlwaysCook.
	 */
	inline UFont* Font()
	{
		static const FSoftObjectPath Path(TEXT("/FlexXR/UI/Fonts/F_Montserrat.F_Montserrat"));

		// Weak pointer so the asset isn't rooted and can still be reimported.
		static TWeakObjectPtr<UFont> Cached;

		if (!Cached.IsValid())
		{
			Cached = Cast<UFont>(Path.TryLoad());
		}

		return Cached.Get();
	}

	/**
	 * The design sizes are CSS pixels but UMG font sizes are points. Slate renders at 96 DPI
	 * (FontCache.cpp), so 1 pt = 4/3 px. Without this every font came out a third too big and the
	 * report's last rows got pushed off the panel. The tokens stay in pixels and get converted here.
	 */
	constexpr float PxToPt = 72.f / 96.f;

	/**
	 * A font at a size in pixels and a weight. Letter spacing is in 1/1000 em (0.14em = 140), so it
	 * doesn't need converting. Large headings use negative tracking so they don't look loose.
	 */
	inline FSlateFontInfo Type(float SizePx, FName Weight, int32 Tracking = 0)
	{
		FSlateFontInfo Info(Font(), SizePx * PxToPt, Weight);
		Info.LetterSpacing = Tracking;
		return Info;
	}

	//~ Type scale, named by use rather than size.
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

	/**
	 * Montserrat's natural line height: (968 + 251 + 0) / 1000 em, from the hhea table (same for all
	 * three weights). CSS line-height multiplies the font size, but UMG's Line Height Percentage
	 * multiplies the natural line, so CSS values have to be divided by this or wrapped text comes out
	 * about 20% too loose.
	 */
	constexpr float NaturalLine = 1.219f;

	/** Line heights from the design (multiples of font size), converted for UMG. */
	constexpr float LineBody      = 1.52f / NaturalLine;
	constexpr float LineRow       = 1.30f / NaturalLine;
	constexpr float LineRowActive = 1.26f / NaturalLine;
	constexpr float LineReason    = 1.40f / NaturalLine;
	constexpr float LineMistake   = 1.20f / NaturalLine;
	constexpr float LineHeading   = 1.10f / NaturalLine;
	constexpr float LineTitle     = 1.06f / NaturalLine;

	// -- brushes ---------------------------------------------------------------------------------
	//
	// Rounded Box brushes, so no nine-slice textures are needed. Pills use Half Height Radius so the
	// radius always matches the height.

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

	/** A pill: radius follows the height. */
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

	/** A circle: a pill with equal width and height. */
	inline FSlateBrush Dot(const FLinearColor& Colour, float Diameter)
	{
		FSlateBrush Brush = Pill(Colour);
		Brush.ImageSize = FVector2D(Diameter, Diameter);
		return Brush;
	}

	/**
	 * A flat band (dividers, meter segments), Thickness tall.
	 *
	 * The size has to go in the brush. UImage::SetDesiredSizeOverride only forwards to the live Slate
	 * widget and doesn't store anything, so on images built in code (not constructed yet) it does
	 * nothing and you get the default 32 x 32. That's how all the 3 px dividers ended up 32 px tall.
	 *
	 * Width is a dummy 1; bands always sit in a slot that fills horizontally.
	 */
	inline FSlateBrush Band(const FLinearColor& Colour, float Thickness = RuleW)
	{
		FSlateBrush Brush;
		Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
		Brush.TintColor = FSlateColor(Colour);
		Brush.ImageSize = FVector2D(1.f, Thickness);
		Brush.OutlineSettings.CornerRadii = FVector4(0.f, 0.f, 0.f, 0.f);
		Brush.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
		return Brush;
	}

	/** Pill track with a pill fill, used for every bar on the panel. */
	inline FProgressBarStyle BarStyle(const FLinearColor& TrackColour, const FLinearColor& FillColour)
	{
		FProgressBarStyle Style;
		Style.BackgroundImage = Pill(TrackColour);
		Style.FillImage = Pill(FillColour);
		Style.EnableFillAnimation = false;
		return Style;
	}

	/**
	 * A button face. Hover and press only brighten it rather than changing the hue, since with a laser
	 * pointer there's no cursor and a colour change would look like a different kind of button.
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

	/** Seconds as m:ss. Used by every clock on the panel. */
	inline FText Clock(float Seconds)
	{
		const int32 Whole = FMath::Max(0, FMath::FloorToInt32(Seconds));
		return FText::FromString(FString::Printf(TEXT("%d:%02d"), Whole / 60, Whole % 60));
	}
}
