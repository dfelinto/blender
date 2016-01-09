"""
Video Capture with DeckLink
+++++++++++++++++++++++++++
Video frames captured with DeckLink cards have pixel formats that are not directly usable by OpenGL,
they must be processed by a shader. Two shaders are presented here, for the 'r210' and 'v210' pixel
formats:
"""
import bge
from bge import logic
from bge import texture as vt

fragment_shaders = {
'r210': """
#version 130
uniform usampler2D tex;
// stereo = 1.0 if 2D image, =0.5 if 3D (left eye below, right eye above)
uniform float stereo;
// eye = 0.0 for the left eye, 0.5 for the right eye
uniform float eye;

void main(void)
{
   vec4 color;
   unsigned int r;
   float tx, ty;
   tx = gl_TexCoord[0].x;
   ty = eye+gl_TexCoord[0].y*stereo;
   r = texture(tex, vec2(tx, ty)).r;
   color.b = float((((r >> 24U)&0xFFU)+((r>>8U)&0x300U))-64U)*0.001136364;
   color.g = float((((r >> 18U)&0x3FU)+((r>>2U)&0x3C0U))-64U)*0.001136364;
   color.r = float((((r >> 12U)&0x0FU)+((r<<4U)&0x3F0U))-64U)*0.001136364;
   // Set alpha to 0.7 for partial transparency when GL_BLEND is enabled
   color.a = 0.7;
   gl_FragColor = color;
}
""",

'v210':"""
#version 130
uniform usampler2D tex;
// stereo = 1.0 if 2D image, =0.5 if 3D (left eye below, right eye above)
uniform float stereo;
// eye = 0.0 for the left eye, 0.5 for the right eye
uniform float eye;

void main(void)
{
   vec4 color;
   float tx, ty;
   float width, sx, dx, Y, U, V, bx;
   unsigned int w0, w1, w2, w3;
   int px;
   
   tx = gl_TexCoord[0].x;
   ty = eye+gl_TexCoord[0].y*stereo;
   // size of the texture (=real size * 2/3)
   width = float(textureSize(tex, 0).x);
   // to sample macro pixels (6 pixels in 4 words)
   sx = tx*width*0.25+0.05;
   // index of display pixel in the macro pixel 0..5
   px = int(floor(fract(sx)*6.0));
   // increment as we sample the macro pixel
   dx = 1.0/width;
   // base x coord of macro pixel
   bx = (floor(sx)+0.01)*dx*4.0;
   // load the 4 words components of the macro pixel
   w0 = texture(tex, vec2(bx, ty)).r;
   w1 = texture(tex, vec2(bx+dx, ty)).r;
   w2 = texture(tex, vec2(bx+dx*2.0, ty)).r;
   w3 = texture(tex, vec2(bx+dx*3.0, ty)).r;
   switch (px) {
   case 0:
   case 1:
      U = float( w0      &0x3FFU);
      V = float((w0>>20U)&0x3FFU);
      break;
   case 2:
   case 3:
      U = float((w1>>10U)&0x3FFU);
      V = float( w2      &0x3FFU);
      break;
   default:
      U = float((w2>>20U)&0x3FFU);
      V = float((w3>>10U)&0x3FFU);
      break;
   }
   switch (px) {
   case 0:
      Y = float((w0>>10U)&0x3FFU);
      break;
   case 1:
      Y = float( w1      &0x3FFU);
      break;
   case 2:
      Y = float((w1>>20U)&0x3FFU);
      break;
   case 3:
      Y = float((w2>>10U)&0x3FFU);
      break;
   case 4:
      Y = float( w3      &0x3FFU);
      break;
   default:
      Y = float((w3>>20U)&0x3FFU);
      break;
   }
   Y = (Y-64.0)*0.001141553;
   U = (U-64.0)*0.001116071-0.5;
   V = (V-64.0)*0.001116071-0.5;
   color.r = Y + 1.5748 * V;
   color.g = Y - 0.1873 * U - 0.4681 * V;
   color.b = Y + 1.8556 * U;
   color.a = 0.7;
   gl_FragColor = color;
}
"""
}

#
# Helper function to attach a pixel shader to the material that receives the video frame.
#
def add_shader(obj, pixel, matId, is_3D):
    if pixel in fragment_shaders:
        frag = fragment_shaders[pixel]
        mat = obj.meshes[0].materials[matId]
        shader = mat.getShader()
        if shader != None and not shader.isValid():
            shader.setSource("", frag, 1)
            shader.setUniform1i("tex", 0)
            shader.setUniformEyef("eye")
            shader.setUniform1f("stereo", 0.5 if is_3D else 1.0);

#
# To be called once to initialize the object
#
def init(cont):
    obj = cont.owner
    if not "video" in obj:
        matId = vt.materialID(obj, 'IMvideo.png')
        add_shader(obj, 'r210', matId, True)
        tex = vt.Texture(obj, matId)
        tex.source = vt.VideoDeckLink("HD1080p24/r210/3D", 0)
        tex.source.play()
        obj["video"] = tex

#
# To be called on every frame
#
def play(cont):
    obj = cont.owner
    try:
        obj["video"].refresh(True)
    except:
        pass
