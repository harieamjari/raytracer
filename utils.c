/* utils.c - utilities for raytracing */
/* Copyright (c) 2023 Al-buharie Amjari and others */
/* Released under MIT */

#include "common.h"
#include "math.h"
#include <assert.h>
#include <math.h>
#include <stdlib.h>

extern scene3D scene;

uint8_t u8_getr(rgba_t rgba) { return (uint8_t)(rgba.r * 255.0); }

uint8_t u8_getg(rgba_t rgba) { return (uint8_t)(rgba.g * 255.0); }

uint8_t u8_getb(rgba_t rgba) { return (uint8_t)(rgba.b * 255.0); }

// generate monte carlo ray
vec3D montc_ray(vec3D norm, uint64_t *rng) {
  *rng = *rng * 0x3243f6a8885a308d + 1;
  uint16_t x = *rng >> 48;
  uint16_t y = *rng >> 32;
  uint16_t z = *rng >> 16;
  vec3D randv = normalize((vec3D){
      x / (float)0x8000 - 1,
      y / (float)0x8000 - 1,
      z / (float)0x8000 - 1,
  });

  if (dot_product(randv, norm) < 0.0)
    return muls_vec3D(randv, -1.0);

  return randv;
}

vec3D rand_unit1(uint64_t *rng) {
  *rng = *rng * 0x3243f6a8885a308d + 1;
  uint16_t x = *rng >> 48;
  uint16_t y = *rng >> 32;
  uint16_t z = *rng >> 16;
  vec3D randv = normalize((vec3D){
      ((float)x / (float)0xFFFF),
      ((float)y / (float)0xFFFF),
      ((float)z / (float)0xFFFF),
  });

  // sum == 1
  return (vec3D){randv.x * randv.x, randv.y * randv.y, randv.z * randv.z};
}

// returns 1 if it interesects an object and initializes
// t, intersect, normal, and object.
// returns 2 if it interesects a light and initializes
// t, intersect, normal, and light.
// returns 0 if max depth is reached, or there's no intersection.
char shoot_ray(vec3D O, vec3D P,
               /* rgba values */
               rgba_t *rgba,
               /* distance from the intersection */
               float *t,
               /* intersection point */
               vec3D *intersect,
               /* surface normal (normalize) */
               vec3D *normal,
               /* triangle of intersection */
               triangle3D *face,
               /* object of intersection */
               object3D *object,
               /* object of intersection */
               light3D *light, char occlusion_test, uint64_t *rng, int depth) {

  if (depth > 2)
    return 0;
  *t = +INFINITY;
  vec3D D = sub_vec3D(P, O);
  char has_intersect = 0;
  for (int object_i = 0; object_i < scene.nb_objects; object_i++) {
    object3D objecti = scene.objects[object_i];
    if (objecti.geometry_type == GEOMETRY_NPRIMITIVE) {
      for (int face_i = 0; face_i < objecti.nb_faces; face_i++) {
        triangle3D facei = objecti.faces[face_i];
        vec3D A = sub_vec3D(*(facei.vertices[0]), O);
        vec3D B = sub_vec3D(*(facei.vertices[1]), O);
        vec3D C = sub_vec3D(*(facei.vertices[2]), O);
        //        vec3D B = *(facei.vertices[1]);
        //        vec3D C = *(facei.vertices[2]);
        matrix3x4 matrix = (matrix3x4){
            (vec4D){A.x - C.x, B.x - C.x, -(P.x - O.x), -C.x},
            (vec4D){A.y - C.y, B.y - C.y, -(P.y - O.y), -C.y},
            (vec4D){A.z - C.z, B.z - C.z, -(P.z - O.z), -C.z},
        };

        vec3D v = {0.0, 0.0, 0.0};
        char no_solution = solve_matrix(matrix, &v);
        if (no_solution)
          continue;
        /* not in the triangle */
        if (v.x < 0.0 || v.y < 0.0 || v.z < 0.0 || v.x + v.y > 1.0)
          continue;

        // vec3D temp_intersect = add_vec3D(add_vec3D(muls_vec3D(A, v.x),
        // muls_vec3D(B, v.y)), muls_vec3D(C, 1.0-(v.x + v.y)));
        vec3D temp_intersect = muls_vec3D(sub_vec3D(P, O), v.z);
        float d = magnitude(temp_intersect);
        // fprintf(stderr, "tr %f\n", d);
        if (*t > d) {
          has_intersect = 1;
          *t = d;
          //*intersect = add_vec3D(add_vec3D(muls_vec3D(A, v.x), muls_vec3D(B,
          //v.y)), muls_vec3D(C, 1.0-(v.x + v.y)));
          *intersect = add_vec3D(temp_intersect, O);
          *face = facei;
          *object = objecti;
          *normal = compute_snormal(facei);
        }
      }
    } else if (objecti.geometry_type == GEOMETRY_SPHERE) {
      vec3D L = sub_vec3D(objecti.sphere_center, O);
      // vec3D L = objecti.sphere_center;
      //        float m = dot_product(normalize(D), L);
      //        if (m  <= 0.0)
      //          continue;
      //	float dc = magnitude(L);
      //	float d;
      //	if ((d = dc*dc - m*m) <= 0.0) {
      //          continue;
      //        }
      //	float p0 = 0.0;
      //	if ((p0 = objecti.sphere_r * objecti.sphere_r - d) <= d)
      //	  continue;
      //
      //        p0 = -sqrtf(p0) + m;
      //
      float tc = dot_product(L, normalize(D));

      if (tc < 0.0)
        continue;
      float d2 = magnitude(L) * magnitude(L) - tc * tc;

      float radius2 = objecti.sphere_r * objecti.sphere_r;
      if (d2 > radius2)
        continue;

      // solve for t1c
      float t1c = sqrtf(radius2 - d2);

      // solve for intersection points
      float t1;
      if (t1c < tc)
        t1 = tc - t1c;
      else
        t1 = tc + t1c;
      if (*t > t1) {
        has_intersect = 1;
        *t = t1;
        *intersect = add_vec3D(muls_vec3D(normalize(D), t1), O);
        *object = objecti;
        //      	*normal = normalize(sub_vec3D(*intersect,
        //      objecti.sphere_center));
        *normal = normalize(sub_vec3D(sub_vec3D(*intersect, O),
                                      sub_vec3D(objecti.sphere_center, O)));
        // fprintf(stderr, "norm %f\n", *normal);
      }
    }
  }
  // check lights
  for (int light_i = 0; light_i < scene.nb_lights; light_i++) {
    light3D lighti = scene.lights[light_i];
    if (lighti.geometry_type == GEOMETRY_NPRIMITIVE) {
      for (int face_i = 0; face_i < lighti.nb_faces; face_i++) {
        triangle3D facei = lighti.faces[face_i];
        vec3D A = sub_vec3D(*(facei.vertices[0]), O);
        vec3D B = sub_vec3D(*(facei.vertices[1]), O);
        vec3D C = sub_vec3D(*(facei.vertices[2]), O);

        matrix3x4 matrix = (matrix3x4){
            (vec4D){A.x - C.x, B.x - C.x, -(P.x - O.x), -C.x},
            (vec4D){A.y - C.y, B.y - C.y, -(P.y - O.y), -C.y},
            (vec4D){A.z - C.z, B.z - C.z, -(P.z - O.z), -C.z},
        };

        vec3D v = {0.0, 0.0, 0.0};
        char no_solution = solve_matrix(matrix, &v);
        if (no_solution)
          continue;
        /* not in the triangle */
        if (v.x < 0.0 || v.y < 0.0 || v.z < 0.0 || v.x + v.y > 1.0)
          continue;

        vec3D temp_intersect = muls_vec3D(sub_vec3D(P, O), v.z);
        float d = magnitude(temp_intersect);
        // fprintf(stderr, "tr %f\n", d);
        if (*t > d) {
          has_intersect = 2;
          *t = d;
          *intersect = add_vec3D(temp_intersect, O);
          *face = facei;
          *light = lighti;
          *normal = compute_snormal(facei);
        }
      }
    } else if (lighti.geometry_type == GEOMETRY_SPHERE) {
      vec3D L = sub_vec3D(lighti.sphere_center, O);
      float tc = dot_product(L, normalize(D));

      if (tc < 0.0)
        continue;
      float d2 = magnitude(L) * magnitude(L) - tc * tc;

      float radius2 = lighti.sphere_r * lighti.sphere_r;
      if (d2 > radius2)
        continue;

      // solve for t1c
      float t1c = sqrtf(radius2 - d2);

      // solve for intersection points
      float t1 = tc - t1c;
      if (t1c > tc)
        continue;
      if (*t > t1) {
        has_intersect = 2;
        *t = t1;
        *intersect = add_vec3D(muls_vec3D(normalize(D), t1), O);
        *light = lighti;
        *normal = normalize(sub_vec3D(*intersect, lighti.sphere_center));
      }
    }
  }
  // we have intersected a light
  if (has_intersect == 2) {
    if (dot_product(normalize(D), *normal) > 0.0)
      return 0;
    //    float val = powf(M_E, -(*t/light->light_intensity));
    //    rgba->r = light->light_color.r * val;
    //    rgba->g = light->light_color.g * val;
    //    rgba->b = light->light_color.b * val;
    rgba->r = light->light_color.r;
    rgba->g = light->light_color.g;
    rgba->b = light->light_color.b;
    rgba->a = 1.0;

    return 2;
  }
  const float lum =
      (dot_product(
           normalize(sub_vec3D(scene.lights[0].sphere_center, *intersect)),
           *normal) +
       1.0) /
      2.0;

  /* apply default color material */
  if (object->material.material_type == MATERIAL_CHECKER_BOARD) {
    if (((int)fabs(intersect->x) & 1 && (int)intersect->z % 2) ||
        (!((int)intersect->x % 2) && !((int)intersect->z % 2))) {
      rgba->r = 0.9;
      rgba->g = 0.9;
      rgba->b = 0.9;
      rgba->a = 1.0;
    } else {
      rgba->r = 0.2;
      rgba->g = 0.2;
      rgba->b = 0.2;
      rgba->a = 1.0;
    }
  } else if (object->material.material_type == MATERIAL_RGB) {
    *rgba = object->material.color;
  }
  // no intersection
  if (has_intersect == 0 || occlusion_test)
    return has_intersect;

  rgba_t di_total_illum = (rgba_t){0.0, 0.0, 0.0, 1.0};
  rgba_t in_total_illum = (rgba_t){0.0, 0.0, 0.0, 1.0};
  /* rgba values */
  rgba_t drgba;
  /* distance from the intersection */
  float dt;
  /* intersection point */
  vec3D dintersect;
  vec3D dnormal;
  /* surface normal (normalize) */
  triangle3D dface;
  object3D dobject;
  light3D dlight;
  // vec3d rl = sub_vec3d(sub_vec3d(scene.lights[0].sphere_center, o),
  // sub_vec3d(*intersect, o));
  for (int light_i = 0; light_i < scene.nb_lights; light_i++) {
    if (scene.lights[light_i].geometry_type == GEOMETRY_SPHERE) {
      vec3D rl = sub_vec3D(scene.lights[light_i].sphere_center, *intersect);
      float dl = magnitude(rl);
      //   vec3D rlo = sub_vec3D(*intersect, scene.lights[0].sphere_center);
      vec3D tnrl = normalize(rl);

#define SFSAMPLE 20
      for (int i = 0; i < SFSAMPLE; i++) {
        vec3D nrl =
            muls_vec3D(montc_ray(tnrl, rng), scene.lights[light_i].sphere_r);

        //*rgba = object->material.color;
        char sret = shoot_ray(add_vec3D(*intersect, muls_vec3D(nrl, 0.1)),
                              add_vec3D(scene.lights[light_i].sphere_center,
                                        nrl) /*add_vec3D(*intersect, nrl)*/,
                              &drgba, &dt, &dintersect, &dnormal, &dface,
                              &dobject, &dlight, 1, rng, depth + 1);
        if (sret == 2 /*sret == 1 */) {
          //    fprintf(stderr, "GEOMETRY LIGHT\n");
          //   float val = powf(M_E, -dt/scene.lights[0].light_intensity);
          // total_illum = add_rgb(total_illum, (rgba_t){val, val, val, 1.0});
          //        di_total_illum = (rgba_t){
          //          val*scene.lights[0].light_color.r + di_total_illum.r,
          //          val*scene.lights[0].light_color.g + di_total_illum.g,
          //          val*scene.lights[0].light_color.b + di_total_illum.b,
          //	  1.0
          //        };
          di_total_illum =
              (rgba_t){drgba.r + di_total_illum.r, drgba.g + di_total_illum.g,
                       drgba.b + di_total_illum.b, 1.0};
        }
      }
      di_total_illum = muls_rgb(di_total_illum, 1.0 / (float)SFSAMPLE);
    } else if (scene.lights[light_i].geometry_type == GEOMETRY_NPRIMITIVE) {
      int face_i = 0;
      triangle3D facei = (scene.lights[light_i].faces[face_i]);

#define DISAMPLE 20
      for (int i = 0; i < DISAMPLE; i++) {
        vec3D randuv = rand_unit1(rng);
        vec3D ra =
            muls_vec3D(sub_vec3D(*facei.vertices[0], *intersect), randuv.x);
        vec3D rb =
            muls_vec3D(sub_vec3D(*facei.vertices[1], *intersect), randuv.y);
        vec3D rc =
            muls_vec3D(sub_vec3D(*facei.vertices[2], *intersect), randuv.z);
        vec3D Di = add_vec3D(ra, add_vec3D(rb, rc));
        vec3D nrl = normalize(Di);

        //*rgba = object->material.color;
        char sret =
            shoot_ray(add_vec3D(*intersect, muls_vec3D(nrl, 0.1)),
                      add_vec3D(Di, *intersect), &drgba, &dt, &dintersect,
                      &dnormal, &dface, &dobject, &dlight, 1, rng, depth + 1);
        if (sret == 2 /*sret == 1 */) {
          //    fprintf(stderr, "GEOMETRY LIGHT\n");
          float val = powf(M_E, -dt / scene.lights[0].light_intensity);
          //            di_total_illum = (rgba_t){
          //              val*scene.lights[light_i].light_color.r +
          //              di_total_illum.r,
          //              val*scene.lights[light_i].light_color.g +
          //              di_total_illum.g,
          //              val*scene.lights[light_i].light_color.b +
          //              di_total_illum.b,
          //    	  1.0
          //            };
          di_total_illum = (rgba_t){val * drgba.r + di_total_illum.r,
                                    val * drgba.g + di_total_illum.g,
                                    val * drgba.b + di_total_illum.b, 1.0};
        }
      }
      di_total_illum = muls_rgb(di_total_illum, 1.0 / (float)DISAMPLE);
    }
  }
#undef DISAMPLE
  for (int i = 0; i < 10; i++) {
    vec3D randv = montc_ray(*normal, rng);
    vec3D randp = muls_vec3D(randv, 2.0);
    if (shoot_ray(add_vec3D(*intersect, muls_vec3D(randp, 0.01)),
                  add_vec3D(*intersect, randp), &drgba, &dt, &dintersect,
                  &dnormal, &dface, &dobject, &dlight, 0, rng,
                  depth + 1) == 1) {
      float val = powf(M_E, -dt / scene.lights[0].light_intensity);
      float wn = dot_product(randv, *normal);
      in_total_illum = (rgba_t){/*val*/ drgba.r * wn + in_total_illum.r,
                                /*val*/ drgba.g * wn + in_total_illum.g,
                                /*val*/ drgba.b * wn + in_total_illum.b, 1.0};
      //        in_total_illum = (rgba_t){
      //          drgba.r + in_total_illum.r,
      //          drgba.g + in_total_illum.g,
      //          drgba.b + in_total_illum.b, 1.0};
    }
  }
  //  in_total_illum = muls_rgb(in_total_illum, 1.0/10.0);

  //}
  vec3D I = muls_vec3D(sub_vec3D(*intersect, O), -1.0);
  // vec3D R = add_vec3D(I, muls_vec3D(*normal, -2.0*dot_product(*normal, I)));
  vec3D R = sub_vec3D(muls_vec3D(*normal, 2.0 * dot_product(*normal, I)), I);
  if (object->reflection_coef != 0.0 &&
      shoot_ray(*intersect, add_vec3D(add_vec3D(R, O), *intersect), &drgba, &dt,
                &dintersect, &dnormal, &dface, &dobject, &dlight, 0, rng,
                depth + 1)) {
    di_total_illum.r += drgba.r * object->reflection_coef;
    di_total_illum.g += drgba.g * object->reflection_coef;
    di_total_illum.b += drgba.b * object->reflection_coef;
    di_total_illum.a = 1.0;
  }
  *rgba = mul_rgb(*rgba, (rgba_t){di_total_illum.r + in_total_illum.r,
                                  di_total_illum.g + in_total_illum.g,
                                  di_total_illum.b + in_total_illum.b, 1.0});
  rgba->a = 1.0;

  /*******************************\
  |  CAUTION: DEAD CODE AHEAD!!!  |
  `------------------------------*/

  return has_intersect;
}
