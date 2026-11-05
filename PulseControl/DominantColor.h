#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

// Pure colour analysis for PulseControl's "Input" colour source. Takes the tiny RGBA8 thumbnail
// read back from the GPU and reduces it to one colour. No GL, no JUCE, so it can be unit tested
// and benchmarked on its own.
namespace pulsecolor
{
	struct Rgb
	{
		float r = 0.0f;
		float g = 0.0f;
		float b = 0.0f;
	};

	struct Result
	{
		Rgb colour;
		// Share of the colourfulness weight that sits in the winning hue region, 0..1. Low means
		// the picture has no clear dominant hue (rainbow, noise); 0 means it was (near) greyscale
		// and the colour is a plain average.
		float coverage = 0.0f;
	};

	// Straight (not premultiplied) average, weighted by alpha. Resolume hands effects
	// premultiplied RGBA, so the colour sums are already alpha-weighted.
	inline Rgb averageColour( const uint8_t* rgba, int numPixels )
	{
		double r = 0.0, g = 0.0, b = 0.0, a = 0.0;
		for ( int i = 0; i < numPixels; ++i )
		{
			r += rgba[ i * 4 + 0 ];
			g += rgba[ i * 4 + 1 ];
			b += rgba[ i * 4 + 2 ];
			a += rgba[ i * 4 + 3 ];
		}
		if ( a <= 0.0 )
			return {};
		return { float( std::min( 1.0, r / a ) ), float( std::min( 1.0, g / a ) ), float( std::min( 1.0, b / a ) ) };
	}

	// Exponential smoothing step, so the colour glides instead of flickering with every frame of video.
	inline Rgb smoothToward( const Rgb& current, const Rgb& target, float amount )
	{
		return { current.r + ( target.r - current.r ) * amount,
				 current.g + ( target.g - current.g ) * amount,
				 current.b + ( target.b - current.b ) * amount };
	}

	// Whether `b` has moved far enough from `a` to be worth another UDP packet and a UI repaint in Pulse.
	inline bool differsEnough( const Rgb& a, const Rgb& b, float threshold )
	{
		return std::max( { std::abs( a.r - b.r ), std::abs( a.g - b.g ), std::abs( a.b - b.b ) } ) >= threshold;
	}

	namespace detail
	{
		constexpr int numHueBins = 36;

		struct Bin
		{
			double weight = 0.0;
			double r = 0.0;
			double g = 0.0;
			double b = 0.0;
		};
	}

	// Dominant colour: a hue histogram weighted by saturation * brightness * alpha, so vivid
	// pixels vote and black/white/grey ones barely do. The winning bin is smoothed with its two
	// neighbours (so a hue sitting on a bin edge isn't split), and the output colour is the weighted
	// mean of the pixels in those three bins. A plain average goes muddy on any multi-colour image.
	inline Result dominantColour( const uint8_t* rgba, int numPixels )
	{
		using namespace detail;

		std::array<Bin, numHueBins> bins{};
		double totalWeight = 0.0;

		for ( int i = 0; i < numPixels; ++i )
		{
			const float a = rgba[ i * 4 + 3 ] / 255.0f;
			if ( a < 0.02f )
				continue;

			// un-premultiply
			const float r = std::min( 1.0f, rgba[ i * 4 + 0 ] / 255.0f / a );
			const float g = std::min( 1.0f, rgba[ i * 4 + 1 ] / 255.0f / a );
			const float b = std::min( 1.0f, rgba[ i * 4 + 2 ] / 255.0f / a );

			const float hi = std::max( { r, g, b } );
			const float lo = std::min( { r, g, b } );
			const float chroma = hi - lo;
			if ( chroma <= 0.0f )
				continue;

			float hue;
			if ( hi == r )
				hue = ( g - b ) / chroma;
			else if ( hi == g )
				hue = 2.0f + ( b - r ) / chroma;
			else
				hue = 4.0f + ( r - g ) / chroma;
			hue /= 6.0f;
			if ( hue < 0.0f )
				hue += 1.0f;

			// chroma == saturation * brightness
			const double w = double( a ) * chroma;
			auto& bin = bins[ size_t( std::min( numHueBins - 1, int( hue * numHueBins ) ) ) ];
			bin.weight += w;
			bin.r += w * r;
			bin.g += w * g;
			bin.b += w * b;
			totalWeight += w;
		}

		// Almost no colour anywhere: greyscale, or mostly transparent. Averaging is the honest answer.
		if ( totalWeight < 0.01 * numPixels )
			return { averageColour( rgba, numPixels ), 0.0f };

		auto at = [ &bins ]( int i ) -> const Bin& { return bins[ size_t( ( i + numHueBins ) % numHueBins ) ]; };

		int best = 0;
		double bestScore = -1.0;
		for ( int i = 0; i < numHueBins; ++i )
		{
			const double score = at( i - 1 ).weight * 0.5 + at( i ).weight + at( i + 1 ).weight * 0.5;
			if ( score > bestScore )
			{
				bestScore = score;
				best = i;
			}
		}

		double w = 0.0, r = 0.0, g = 0.0, b = 0.0;
		for ( int i = best - 1; i <= best + 1; ++i )
		{
			w += at( i ).weight;
			r += at( i ).r;
			g += at( i ).g;
			b += at( i ).b;
		}

		return { { float( r / w ), float( g / w ), float( b / w ) }, float( w / totalWeight ) };
	}
}
