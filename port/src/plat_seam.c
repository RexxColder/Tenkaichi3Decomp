/* ObjSeam_TransformVtx (0x10FFD0), the one game routine without a C version yet (hand-written VU0 assembly; the
   maths is described in the header of src/sys/gfxm_d_c.c). Until it is written: a model with seam vertices stops
   the game here, or with BT3_SEAM_SKIP=1 is drawn without them (to look at which models these are). */
#include <stdio.h>
#include <stdlib.h>

void ObjSeam_TransformVtx(void) {
    static int said;
    if (getenv("BT3_SEAM_SKIP") == NULL) {
        printf("bt3: not implemented yet: ObjSeam_TransformVtx\n");
        exit(3);
    }
    if (!said) {
        said = 1;
        printf("bt3: seam vertices skipped (ObjSeam_TransformVtx is not written yet)\n");
        fflush(stdout);
    }
}
