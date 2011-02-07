
void HSVtoRGB( int *r, int *g, int *b, int hue, int s, int v )
{
    int f;
    long p, q, t;

    if( s == 0 )
    {
        // easy - just shades of grey.
        *r = *g = *b = v;
        return;
    }
 
    f = ((hue%40)*255)/40;
    hue /= 40;
 
    p = (v * (256 - s))/256;
    q = (v * ( 256 - (s * f)/256 ))/256;
    t = (v * ( 256 - (s * ( 256 - f ))/256))/256;
 
    switch( hue ) {
    case 0:
        *r = v;
        *g = t;
        *b = p;
        break;
    case 1:
        *r = q;
        *g = v;
        *b = p;
        break;
    case 2:
        *r = p;
        *g = v;
        *b = t;
        break;
    case 3:
        *r = p;
        *g = q;
        *b = v;
        break;
    case 4:
        *r = t;
        *g = p;
        *b = v;
        break;
    default:
        *r = v;
        *g = p;
        *b = q;
        break;
    }
}

void convert_hsv_to_rgb_float(float *nr, float *ng, float *nb, float hue, float s, float v)
{
        float p1, p2, p3, i, f;
        float xh;

        if (hue == 240.0)
                hue = 0.0;           /* (THIS LOOKS BACKWARDS)       */

        xh = hue / 40.;                   /* convert hue to be in 0,6       */
        i = (float)floor((double)xh);    /* i = greatest integer <= h    */
        f = xh - i;                     /* f = fractional part of h     */
        p1 = v * (1 - s);
        p2 = v * (1 - (s * f));
        p3 = v * (1 - (s * (1 - f)));

        switch ((int) i)
        {
                case 0:
                        *nr = v;
                        *ng = p3;
                        *nb = p1;
                        break;
                case 1:
                        *nr = p2;
                        *ng = v;
                        *nb = p1;
                        break;
                case 2:
                        *nr = p1;
                        *ng = v;
                        *nb = p3;
                        break;
                case 3:
                        *nr = p1;
                        *ng = p2;
                        *nb = v;
                        break;
                case 4:
                        *nr = p3;
                        *ng = p1;
                        *nb = v;
                        break;
                case 5:
                        *nr = v;
                        *ng = p1;
                        *nb = p2;
                        break;
        }

        return;
}
 
int main(void) {

    int hue = 0;
    int r;
    int g; 
    int b;
    float nr, ng, nb;

    int i;
    for (i=0; i<240; i++) {
        HSVtoRGB(&r, &g, &b, i, 255, 255);
        convert_hsv_to_rgb_float(&nr, &ng, &nb, (float)(i), 1.0, 1.0);
        printf("Int:   R: %3i, G: %3i, B: %3i  :: Hue %3i :: ID %02x\n", r, g, b, i, i+1);
        printf("Float: R: %3.0f, G: %3.0f, B: %3.0f  :: Hue %3i\n", nr*255, ng*255, nb*255, i);
        printf("\n");
    }

}

