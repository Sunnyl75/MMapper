# Matched roof atlas

Generated with the built-in image generation tool. The selected corrected PNG
is committed as `src/resources/pixmaps/terrain-city-eaves-v3.png`.

Fixed role rows: straight run, convex hip, concave valley, flush end board.
Six variants per role. Each source cell is 256 pixels square. The importer
crops transparent margins; strips occupy the top 0.24 of their cell and repeat
at 0.75-room intervals in world coordinates, preserving their approximately
3:1 source aspect rather than stretching them to room width; square
corners retain equal scale on both axes; boards regain their narrow physical
width in the shader. Roles are not seasons.

## Prompts

+Use case: stylized-concept.
Asset type: production modular roof sprite atlas for the MMapper top-down fantasy map, Bree Legends comic style, weathered medieval WOODEN shingles.
Reference image 1: style reference only, its worn brown wood and outlined hand-drawn overlapping shingles; improve proportions and make a matched modular construction kit.
Create ONE transparent RGBA landscape image 1536x1024, exactly SIX columns and FOUR rows, each cell 256x256. No labels, no grid lines, no backdrop, no ground shadows, no perspective/isometric tilt. Absolute overhead orthographic view.
All pieces share the SAME warm grey-brown wood palette, two staggered courses of rough split wooden shingles, understated thin dark ink, clear individual overlapping tips. Moderate surface grain, not photorealistic. No stone, no walls, no gutters, no framing outlines.
ROW 1 (top six cells): six straight horizontal roof strips, each exactly 240px wide and 58px deep, centered in its cell. Each strip has about TWELVE narrow shingles across and TWO courses along depth. Perfectly straight cut at TOP of strip (wall/root side); lower shingle course points DOWN and extends out with gently irregular tips at BOTTOM (street/eave side). Upper shingles overlap the roots of lower ones. NO end boards and NO side outlines; left and right sides end in half shingles for joining. Six very subtle variants only, identical dimensions and course spacing.
ROW 2: six square convex roof HIP corner pieces, each 216x216 centered. These depict the same roof at a corner, with wall root at TOP LEFT and low outer eave at BOTTOM and RIGHT. Hip seam runs top-left to bottom-right, with a NARROW row of small overlapping wooden hip caps. TWO perpendicular roof faces: ABOVE-RIGHT of diagonal, shingles run vertically and point DOWN; BELOW-LEFT of diagonal, the IDENTICAL shingle pattern is rotated 90 degrees, shingles run horizontally and point RIGHT. Each face has TWO staggered courses across its slope, around THREE shingle widths along each full connecting edge. Course line halfway through depth must meet the straight strip's halfway course. Do not put a separate square frame around a piece. Connecting TOP and LEFT edges are flush cuts. Outer BOTTOM and RIGHT edges have subtle shingle tips.
ROW 3: six square concave roof VALLEY corners, each 216x216 centered. Same two perpendicular roof faces and three-shingle edge scale as row 2; valley seam top-left to bottom-right is a narrow recessed dark wooden valley, no raised diagonal log. Here the high cut wall sides are BOTTOM and RIGHT; the roof slopes toward TOP and LEFT, with upper-right face shingles pointing LEFT and lower-left face shingles pointing UP. Same halfway course spacing. No outside square outlines.
ROW 4: six narrow rectangular weathered wood roof END CAPS, each 25px wide by 216px tall centered. Flat square-cut top and bottom, not pointed. Restrained dark bevel on one side and light exposed wood grain, no decorative spear shapes, no bulky outline. Used as very slim flush bargeboards along the depth of the two-course roof.
CRUCIAL: rows 2/3 represent a roof-depth-sized corner, not a room-sized roof. Their shingle widths are about one third of the square side. Row 1 represents a full room length with twelve shingles. Keep all pieces cleanly separate with genuine alpha transparency, not a drawn checkerboard.

CORRECTION:
Use case: precise-object-edit. Edit target: the attached transparent 6-column 4-row wooden roof atlas. Preserve the entire first row of six straight shingle strips and last row of six narrow flat-ended boards EXACTLY, and preserve true alpha transparency, palette, weathered Bree Legends ink style, 1536x1024 size, six columns, four separated rows.
Correct ONLY the twelve square roof corner sprites in rows TWO and THREE:
Row TWO: convex HIP corner, high root at top-left and exposed low eaves along bottom and right. Diagonal hip cap from top-left to bottom-right. In the triangular face ABOVE/RIGHT of diagonal the wood grain and long axes of ALL shingles must be HORIZONTAL and shingle tips point RIGHT. In triangular face BELOW/LEFT of diagonal ALL long axes and grain are VERTICAL and tips point DOWN. This is essential; exchange the current faces' shingle directions. At the TOP edge the shingle course line must be at half the square width (two courses across x), and at LEFT edge course line at half the square height (two courses across y). TWO overlapping staggered courses only in each roof face, not four. Each complete connecting edge has about three shingle widths. Shingles about one-third the square width and one-half its depth. Narrow overlapping hip cap no wider than one-fifth the square.
Row THREE: concave VALLEY corner, high roots along TOP and LEFT, valley seam diagonally top-left to bottom-right. Upper-right triangular face shingles point DOWN with VERTICAL grain; lower-left triangular face shingles point RIGHT with HORIZONTAL grain. BOTH slope away from the top-left root, do NOT point left or up. TWO staggered overlapping courses only across each slope, course boundary halfway down each square, about three shingle widths along an edge. Very slim recessed dark valley seam, no big diagonal plank.
No enclosing frame, no backdrop, no cast shadow. All square corners same dimensions and same palette as first row, clean flush attachment edges and subtle irregular tips on low outer edges. Roof pieces must physically join the straight two-course strips.
