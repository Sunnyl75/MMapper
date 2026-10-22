# Rapids and flow artwork

Generated with the built-in image tool, with true alpha requested and verified.
Source PNGs are bundled unchanged. Qt frames all 24 cells at load time. The
flow shader suppresses the blue-grey wash between bright foam crests so the
existing river water remains visible.

Final assets:
- `src/resources/pixmaps/terrain-rapids-patches.png`
- `src/resources/pixmaps/terrain-water-flow.png`

The original `terrain-rapids.png` legacy room tile is preserved unchanged.

## Rapids prompt

Use case: stylized-concept. Production top-down 2D game terrain sprite atlas, matching an earthy illustrated fantasy comic map, Bree Legends style. One 1536x1024 PNG, EXACT 6 columns by 4 rows, 24 equally spaced square cells. In each cell one different irregular rounded patch of river RAPIDS: lively broken ivory-white foam crests, soft grey-blue swirling strokes and small hints of dark submerged stones. The WATER BACKGROUND IS ABSENT, truly transparent everywhere between foam strokes, with transparent soft tapered edges. Patches should blend over the game's existing teal river water. No square tiles, no background rectangle, no grid, no labels, no text, no checkerboard, no heavy black outlines. Clear readable simplified ink-and-paint comic illustration, restrained muted ivory foam, not luminous/neon. Patches separated in their own cells, shapes occupy about 85 percent of each cell. Same year-round appearance, no snow seasons. Gentle flowing long curved foam streaks, organic silhouettes and variation, seen straight down, no horizon. True alpha transparency.

## Flow prompt

Use case: stylized-concept. Production top-down 2D game WATER FLOW DIRECTION sprite atlas, same earthy ink-and-paint fantasy comic map style as Bree Legends. One transparent 1536x1024 PNG, EXACTLY 6 columns and 4 rows, 24 equally spaced square sprite cells, no grid or text. Each cell has one small isolated NORTHWARD FLOWING WATER glyph, made of natural ivory/pale blue-grey foam wavelets. ALL 24 MUST POINT UP toward TOP of sheet: a compact gently pointed rounded bow-wave at its northern leading tip and two curved trailing ripples fanning outward to the south, plus 2 or 3 thin tapering wake strokes trailing straight SOUTH behind it. Make the leading northern tip and southern wake clearly asymmetric so flow direction is unambiguous even when small. NOT a geometric arrow, NOT a V icon, NOT a fish, NOT a boat, no arrow shaft; use believable stylised ripples and flowing foam. Clean broad curved tapered strokes, minimal ink contour, readable at 32 pixels. No surrounding blue water tile, all spaces between ripple strokes genuinely transparent alpha. Each motif centred within its own cell with generous transparent margin; identical orientation and scale in all cells, 24 subtle variants. Neutral ivory/blue-grey, no vivid blue, no glow, no cast shadow. Straight down overhead, flat lighting.

## Flow refinement prompt

Edit this 6x4 sprite atlas: replace every motif with a small NORTHWARD travelling water ripple, preserving the exact grid, alpha transparency, and overall 24-variant layout. The existing motifs look like snow-capped mountains or solid arrowheads: remove ALL the filled triangular blue wedges. Each replacement must be made of ONLY THREE SEPARATE THIN CURVED FOAM CRESTS with fully transparent open space between them. Each crest is like a gently pointed rounded arch / upturned eyebrow whose small centre tip is at NORTH/TOP and whose two ends sweep back SOUTH. Arrange the three curved crests one behind another vertically with thin clear transparent gaps; front crest small and narrow at top, two crests behind wider. A few subtle vertical wake dashes trailing south below. No large filled area of blue or grey, no mountain silhouette, no solid triangle, no arrow shaft. Ivory and muted pale blue-grey strokes, airy broken ripples, seen straight down; each occupies a small central area within its cell. True alpha background, no painted background or glow. Keep all24 pointing UP and 6 equal columns by4 equal rows. This is readable river-current foam, not snow or mountains.
