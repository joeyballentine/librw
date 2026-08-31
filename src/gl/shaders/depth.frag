// The caster pass: write depth into an ordinary colour target.
//
// Paired with default.vert or skin.vert unmodified. It reads none of their
// outputs -- a fragment shader may declare fewer inputs than the vertex shader
// writes -- so casters need no second set of vertex shaders and skinned casters
// come for free.
//
// gl_FragCoord.z and not a varying, which is what makes that true. Under a
// PERSPECTIVE projection that value is hyperbolic and packing it would waste
// most of its precision near the far plane. The light camera is ORTHOGRAPHIC,
// where it is linear in world distance, so it is exactly the quantity wanted.
// A perspective light camera here would silently lose precision rather than
// fail, which is why the projection is a decision recorded in docs/SHADOWS.md
// rather than a detail.

// 24 bits of depth spread over three bytes.
//
// The alpha byte is deliberately left at 1.0. A packed value has to survive the
// round trip through a render target that something else may blend against, and
// on the D3D9 side the port infers whether a draw carries alpha from the
// material -- a zero there is a trap for later.
//
// Note for anyone editing this file: it becomes a C string literal one line at
// a time, through a sed recipe in the Makefile that does not escape anything.
// A double quote character here is a compile error in the generated .inc.
vec4
PackDepth(float d)
{
	// The multiply ladder and the subtract are the standard encode. Each
	// channel keeps the fraction the ones above it did not.
	vec3 bits = vec3(1.0, 255.0, 65025.0) * d;
	bits = fract(bits);
	// Take back what the next channel up is going to represent, or the sum
	// decodes about one part in 255 too high on every channel.
	bits -= bits.yzz * vec3(1.0/255.0, 1.0/255.0, 0.0);
	return vec4(bits, 1.0);
}

void
main(void)
{
	FRAGCOLOR(PackDepth(gl_FragCoord.z));
}
