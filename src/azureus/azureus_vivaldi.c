/***************************************************************************
 *  Copyright (C) 2007 by Saritha Kalyanam                                 *
 *  kalyanamsaritha@gmail.com                                              *
 *                                                                         *
 *  This program is free software: you can redistribute it and/or modify   *
 *  it under the terms of the GNU Affero General Public License as         *
 *  published by the Free Software Foundation, either version 3 of the     *
 *  License, or (at your option) any later version.                        *
 *                                                                         *
 *  This program is distributed in the hope that it will be useful,        *
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of         *
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the          *
 *  GNU Affero General Public License for more details.                    *
 *                                                                         *
 *  You should have received a copy of the GNU Affero General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.  *
 ***************************************************************************/

#include <string.h>

#include "azureus_vivaldi.h"

#include "float.h"
#include "crypto.h"
#include "azureus_rpc.h"

static const float initial_err = 10.0f;
static const float cc = 0.25f;
static const float ce = 0.5f;
static int nb_updates = 0;
static const int CONVERGE_EVERY = 5;
static const float CONVERGE_FACTOR = 50.0f;
static const float ERROR_MIN = 0.1f;
static const float MAX_X = 30000.0f;
static const float MAX_Y = 30000.0f;
static const float MAX_H = 30000.0f;

static int azureus_vivaldi_v1_encode(struct pkt *pkt, 
                                        struct azureus_vivaldi_pos *pos);
static int azureus_vivaldi_v2_encode(struct pkt *pkt, 
                                        struct azureus_vivaldi_pos *pos);
static int azureus_vivaldi_v1_decode(struct pkt *pkt, 
                                        struct azureus_vivaldi_pos *pos);

static float azureus_vivaldi_v1_measure(struct azureus_vivaldi_pos *pos);

int
azureus_vivaldi_encode(struct pkt *pkt, int type, 
                                        struct azureus_vivaldi_pos *pos)
{
    int ret;

    ASSERT(pkt && pos);

    ret = pkt_mark(pkt, 512);
    if (ret != SUCCESS) {
        return ret;
    }

    switch (type) {
        case POSITION_TYPE_VIVALDI_V1:
            ret = azureus_vivaldi_v1_encode(pkt, pos);
            if (ret != SUCCESS) {
                return ret;
            }
            break;

        case POSITION_TYPE_VIVALDI_V2:
            ret = azureus_vivaldi_v2_encode(pkt, pos);
            if (ret != SUCCESS) {
                return ret;
            }
            break;
            
        default:
            pkt_reset(pkt);
            return FAILURE;
    }

    return SUCCESS;
}

static int
azureus_vivaldi_v1_encode(struct pkt *pkt, struct azureus_vivaldi_pos *pos)
{
    float f[4];
    int i;
    int ret;

    ASSERT(pkt && pos);
    ASSERT(pos->type == POSITION_TYPE_VIVALDI_V1);

    f[X] = pos->v.v1.x;
    f[Y] = pos->v.v1.y;
    f[H] = pos->v.v1.h;
    f[E] = pos->v.v1.err;

    for (i = 0; i < 4; i++) {
        ret = pkt_write_float(pkt, f[i]);
        if (ret != SUCCESS) {
            return ret;
        }
    }

    return SUCCESS;
}

static int
azureus_vivaldi_v2_encode(struct pkt *pkt, struct azureus_vivaldi_pos *pos)
{
    double f[4];
    int i;
    int ret;

    ASSERT(pkt && pos);
    ASSERT(pos->type == POSITION_TYPE_VIVALDI_V2);

    f[X] = pos->v.v2.x;
    f[Y] = pos->v.v2.y;
    f[H] = pos->v.v2.h;
    f[E] = pos->v.v2.err;

    ret = pkt_write_byte(pkt, pos->v.v2.n_coords);
    if (ret != SUCCESS) {
        return ret;
    }

    for (i = 0; i < 4; i++) {
        ret = pkt_write_double(pkt, f[i]);
        if (ret != SUCCESS) {
            return ret;
        }
    }

    return SUCCESS;
}

int
azureus_vivaldi_decode(struct pkt *pkt, int type, 
                            struct azureus_vivaldi_pos *pos)
{
    int ret;

    ASSERT(pkt && pos);

    bzero(pos, sizeof(struct azureus_vivaldi_pos));

    ret = pkt_mark(pkt, 512);
    if (ret != SUCCESS) {
        return ret;
    }

    switch (type) {
        case POSITION_TYPE_VIVALDI_V1:
            ret = azureus_vivaldi_v1_decode(pkt, pos);
            if (ret != SUCCESS) {
                return ret;
            }
            break;

        case POSITION_TYPE_VIVALDI_V2:
            return FAILURE;

        default:
            pkt_reset(pkt);
            return FAILURE;
    }

    return SUCCESS;
}

static int
azureus_vivaldi_v1_decode(struct pkt *pkt, struct azureus_vivaldi_pos *pos)
{
    int i;
    int ret;
    float f[4];

    ASSERT(pkt && pos);

    pos->type = POSITION_TYPE_VIVALDI_V1;

    for (i = 0; i < 4; i++) {
        ret = pkt_read_float(pkt, &f[i]);
        if (ret != SUCCESS) {
            return ret;
        }
    }

    pos->v.v1.x = f[X];
    pos->v.v1.y = f[Y];
    pos->v.v1.h = f[H];
    pos->v.v1.err = f[E];

    return SUCCESS;
}

int
azureus_vivaldi_pos_new(struct azureus_vivaldi_pos *pos, u8 type, 
                            float x, float y, float h)
{
    bzero(pos, sizeof(struct azureus_vivaldi_pos));
    switch (type) {
        case POSITION_TYPE_VIVALDI_V1:
            pos->type = POSITION_TYPE_VIVALDI_V1;
            pos->v.v1.x = x;
            pos->v.v1.y = y;
            pos->v.v1.h = h;
            pos->v.v1.err = initial_err;
            break;

        case POSITION_TYPE_VIVALDI_V2:
            pos->type = POSITION_TYPE_VIVALDI_V2;
            pos->v.v2.n_coords = 4;
            pos->v.v2.x = 100.0f;
            pos->v.v2.y = 100.0f;
            pos->v.v2.x = 100.0f;
            pos->v.v2.err = 1.0f;
            break;

        default:
            break;
    }

    return SUCCESS;
}

void
azureus_vivaldi_v1_pos_delete(struct azureus_vivaldi_pos *pos)
{
    ASSERT(pos);

    free(pos);
}

bool
azureus_vivaldi_v1_at_origin(struct azureus_vivaldi_pos *pos)
{
    ASSERT(pos);

    return ((pos->v.v1.x == 0.0f) && (pos->v.v1.y == 0.0f));
}

bool
azureus_vivaldi_v1_is_valid(struct azureus_vivaldi_pos *pos)
{
    ASSERT(pos);

    return float_is_valid(pos->v.v1.x) && 
            float_is_valid(pos->v.v1.y) && 
            float_is_valid(pos->v.v1.h) &&
            (fabs(pos->v.v1.x) <= MAX_X) &&
            (fabs(pos->v.v1.y) <= MAX_Y) &&
            (fabs(pos->v.v1.h) <= MAX_H);
}

int
azureus_vivaldi_v1_add(struct azureus_vivaldi_pos *p1,
                    struct azureus_vivaldi_pos *p2,
                    struct azureus_vivaldi_pos *res)
{
    ASSERT(p1 && p2 && res);

    azureus_vivaldi_pos_new(res, POSITION_TYPE_VIVALDI_V1, 
                            (p1->v.v1.x + p2->v.v1.x), 
                            (p1->v.v1.y + p2->v.v1.y), 
                            fabs(p1->v.v1.h + p2->v.v1.h));

    return SUCCESS;
}

int
azureus_vivaldi_v1_sub(struct azureus_vivaldi_pos *p1,
                    struct azureus_vivaldi_pos *p2,
                    struct azureus_vivaldi_pos *res)
{
    ASSERT(p1 && p2 && res);

    azureus_vivaldi_pos_new(res, POSITION_TYPE_VIVALDI_V1, 
                            (p1->v.v1.x - p2->v.v1.x), 
                            (p1->v.v1.y - p2->v.v1.y), 
                            fabs(p1->v.v1.h + p2->v.v1.h));

    return SUCCESS;
}

int
azureus_vivaldi_v1_scale(struct azureus_vivaldi_pos *pos, float scale)
{
    ASSERT(pos);

    pos->type = POSITION_TYPE_VIVALDI_V1;
    pos->v.v1.x *= scale;
    pos->v.v1.y *= scale;
    pos->v.v1.h *= scale;

    return SUCCESS;
}

int
azureus_vivaldi_v1_unity(struct azureus_vivaldi_pos *pos, 
                            struct azureus_vivaldi_pos *res)
{
    float measure;

    ASSERT(pos && res);

    bzero(res, sizeof(struct azureus_vivaldi_pos));
    *res = *pos;

    measure = azureus_vivaldi_v1_measure(pos);
    if (measure == 0.0f) {
        res->v.v1.x = 1.0*random()/RAND_MAX;
        res->v.v1.y = 1.0*random()/RAND_MAX;
        res->v.v1.h = 1.0*random()/RAND_MAX;

        measure = azureus_vivaldi_v1_measure(res);
        azureus_vivaldi_v1_scale(res, 1.0/measure);

        return SUCCESS;
    }

    azureus_vivaldi_v1_scale(res, 1.0/measure);

    return SUCCESS;
}

bool
azureus_vivaldi_v1_equals(struct azureus_vivaldi_pos *p1, 
                            struct azureus_vivaldi_pos *p2)
{
    ASSERT(p1 && p2);

    if (p1->type == p2->type) {
        if ((p1->v.v1.x != p2->v.v1.x)
                || (p1->v.v1.y != p2->v.v1.y)
                || (p1->v.v1.h != p2->v.v1.h)) {
            return FALSE;
        }

        return TRUE;
    }

    return FALSE;
}

static float
azureus_vivaldi_v1_measure(struct azureus_vivaldi_pos *pos) 
{
    ASSERT(pos);

    return (sqrt((pos->v.v1.x * pos->v.v1.x) + 
                    (pos->v.v1.y * pos->v.v1.y)) + pos->v.v1.h);
}

float 
azureus_vivaldi_v1_distance(struct azureus_vivaldi_pos *p1,
                            struct azureus_vivaldi_pos *p2)
{
    struct azureus_vivaldi_pos sub;

    azureus_vivaldi_v1_sub(p1, p2, &sub);

    return azureus_vivaldi_v1_measure(&sub);
}

float
azureus_vivaldi_v1_estimate_rtt(struct azureus_vivaldi_pos *p1, 
                                struct azureus_vivaldi_pos *p2)
{
    ASSERT(p1 && p2);

    if (azureus_vivaldi_v1_at_origin(p1) || azureus_vivaldi_v1_at_origin(p2)) {
        return ieee754_to_float(NAN_PLUS);
    }

    return azureus_vivaldi_v1_distance(p1, p2);
}

int
azureus_vivaldi_v1_update(struct azureus_vivaldi_pos *pos, float rtt, 
                            struct azureus_vivaldi_pos *cj, float ej)
{
    float w, re, es, new_err, delta, scale;
    struct azureus_vivaldi_pos rnd_err, new_pos, zero;
    float f_x, f_y;
    int ret;
    struct azureus_vivaldi_pos res_1, res_2;

retry:

    if (!float_is_valid(rtt) || !float_is_valid(ej) || 
            !azureus_vivaldi_v1_is_valid(pos) ||
            !azureus_vivaldi_v1_is_valid(cj)) {
        ERROR("Vivaldi update rejected\n");
        return FAILURE;
    }

    if (rtt <= 0.0f || rtt > 5.0*60*1000) {
        ERROR("Invalid RTT\n");
        return FAILURE;
    }

    if ((pos->v.v1.err + ej) == 0.0f) {
        ERROR("Invalid ERROR\n");
        return FAILURE;
    }

    w = pos->v.v1.err/(ej + pos->v.v1.err);
    DEBUG("w = %f\n", w);
    re = rtt - azureus_vivaldi_v1_distance(pos, cj);
    DEBUG("re = %f\n", re);
    es = fabs(re)/rtt;
    DEBUG("es = %f\n", es);
    new_err = es * ce * w + pos->v.v1.err * (1.0f - ce * w);
    DEBUG("new_err = %f\n", new_err);

    delta = cc * w;
    DEBUG("delta = %f\n", delta);
    scale = delta * re;
    DEBUG("scale = %f\n", scale);
    f_x = 1.0*random()/RAND_MAX;
    f_y = 1.0*random()/RAND_MAX;

    DEBUG("f_x = %f f_y = %f\n", f_x, f_y);

    ret = azureus_vivaldi_pos_new(&rnd_err, POSITION_TYPE_VIVALDI_V1, 
                                    f_x/10, f_y/10, 0.0f);
    if (ret != SUCCESS) {
        return ret;
    }

    azureus_vivaldi_pos_dump(&rnd_err);

    /* FIXME: some complex construction */
    azureus_vivaldi_v1_add(cj, &rnd_err, &res_1);
    azureus_vivaldi_v1_sub(pos, &res_1, &res_2);
    azureus_vivaldi_v1_unity(&res_2, &res_1);
    azureus_vivaldi_v1_scale(&res_1, scale);
    azureus_vivaldi_v1_add(pos, &res_1, &new_pos);

    if (float_is_valid(new_err) && azureus_vivaldi_v1_is_valid(&new_pos)) {
        *pos = new_pos;
        pos->v.v1.err = new_err > ERROR_MIN ? new_err : ERROR_MIN;
    } else {
        azureus_vivaldi_pos_new(pos, POSITION_TYPE_VIVALDI_V1, 
                                0.0f, 0.0f, 0.0f);
        pos->v.v1.err = initial_err;
    }
    azureus_vivaldi_pos_dump(pos);

    if (!azureus_vivaldi_v1_at_origin(cj)) {
        nb_updates++;
    }

    if (nb_updates > CONVERGE_EVERY) {
        nb_updates = 0;

        rtt = 10.0f;
        azureus_vivaldi_pos_new(&zero, POSITION_TYPE_VIVALDI_V1, 
                                    0.0f, 0.0f, 0.0f);
        ej = CONVERGE_FACTOR;
        goto retry;
    }

    return SUCCESS;
}

void
azureus_vivaldi_pos_dump(struct azureus_vivaldi_pos *pos)
{
    ASSERT(pos);

    if (pos->type != POSITION_TYPE_VIVALDI_V1) {
        return;
    }

    DEBUG("type:%d x:%f y:%f h:%f err:%f\n", 
                pos->type, 
                pos->v.v1.x, pos->v.v1.y, pos->v.v1.h, pos->v.v1.err);

    return;
}
