# Decorative roof corner overlays

Generated with the built-in image generation tool from the V3 shingle atlas as
a style reference. Selected PNG: `src/resources/pixmaps/terrain-roof-caps.png`.
Three columns, two rows: convex hip covers, then concave valley covers.
The importer puts these into the previously unused roof slots of WALL_FINISH,
preserving the stone pier artwork and the existing 140-layer texture limit.

The renderer maps the art into a roof-depth square with 0.045-room overlap on
all sides. Convex caps are split into three room-owned fragments so their
shoulders cover both adjoining straight runs without entering the indoor room.
Concave caps stay inside their own outdoor room. A separate final batch draws
all caps above the underlying walls and eaves.

## Prompt

+Use case: stylized-concept. Production game sprite atlas. Reference: match the weathered wooden shingles' warm brown/grey palette, restrained ink lines and Bree Legends medieval comic illustration style in the attached source.
Create a transparent RGBA 1536x1024 image, EXACTLY THREE columns and TWO rows, six separate sprites, each centred in a 512x512 square with at least 40 pixels transparent clearance. True top-down overhead orthographic view. No labels, grid, background, floor, scenery, stone or walls.
These are PURPOSE-MADE DECORATIVE CORNER COVERS placed ABOVE an existing wooden shingle roof, with a small overlap. They must read as raised carpentry, not square patches of ordinary shingles. They intentionally hide mismatched joins underneath.
TOP ROW: three subtle variants of a carved wooden OUTSIDE HIP corner cover. Main silhouette is a rounded triangular FAN with two short projecting shoulders/tabs at its TOP edge and LEFT edge, to lap over adjoining roof strips. Root is near TOP LEFT. Central diagonal raised ridge runs from TOP LEFT to BOTTOM RIGHT, made of four chunky overlapping wooden hip-cap shingles with bevelled edges. Two broad fan-shaped side cheeks, with 2-3 simple carved ribs or overlapping wooden scales on each side, spread toward BOTTOM and RIGHT. Gently scalloped/rounded outer edge, gracefully chamfered corners. The shoulders on top and left are integral parts of the wood, not separate rails. Restrained contact shadow immediately under the edge, mostly transparent and only a few pixels wide. No complete square enclosing border, no surrounding rectangular frame, no ordinary rectangular shingle grid across the whole piece. Intentional ornamental woodwork, modest and worn, no heraldry or animals, no metal.
BOTTOM ROW: three subtle variants of a carved wooden INSIDE VALLEY cover. Same matching palette, raised carpentry, overlapping shoulder shape and scale. A shallow recessed narrow diagonal valley from TOP LEFT to BOTTOM RIGHT between two bevelled tapered wood cheeks. Rounded fan/chevron silhouette, little projecting shoulders on BOTTOM and RIGHT to lap over the adjoining roof strips. Mostly continuous wood coverage in centre, no large hole. Scalloped outer lips and subtle carved lines distinguish this purpose-made corner finish from plain shingles. No square frame.
All six same apparent wood scale, bold readable outlines without heavy black rectangles. Each sprite occupies about 400x400 pixels with transparent corners around its organic outline. The top-left origin and diagonal direction must be consistent for all six. Avoid photorealism, perspective, schematic outlines or checkerboards.
