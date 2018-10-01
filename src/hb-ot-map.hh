/*
 * Copyright © 2009,2010  Red Hat, Inc.
 * Copyright © 2010,2011,2012,2013  Google, Inc.
 *
 *  This is part of HarfBuzz, a text shaping library.
 *
 * Permission is hereby granted, without written agreement and without
 * license or royalty fees, to use, copy, modify, and distribute this
 * software and its documentation for any purpose, provided that the
 * above copyright notice and the following two paragraphs appear in
 * all copies of this software.
 *
 * IN NO EVENT SHALL THE COPYRIGHT HOLDER BE LIABLE TO ANY PARTY FOR
 * DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES
 * ARISING OUT OF THE USE OF THIS SOFTWARE AND ITS DOCUMENTATION, EVEN
 * IF THE COPYRIGHT HOLDER HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 *
 * THE COPYRIGHT HOLDER SPECIFICALLY DISCLAIMS ANY WARRANTIES, INCLUDING,
 * BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS FOR A PARTICULAR PURPOSE.  THE SOFTWARE PROVIDED HEREUNDER IS
 * ON AN "AS IS" BASIS, AND THE COPYRIGHT HOLDER HAS NO OBLIGATION TO
 * PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR MODIFICATIONS.
 *
 * Red Hat Author(s): Behdad Esfahbod
 * Google Author(s): Behdad Esfahbod
 */

#ifndef HB_OT_MAP_HH
#define HB_OT_MAP_HH

#include "hb-buffer.hh"


#define HB_OT_MAP_MAX_BITS 8u
#define HB_OT_MAP_MAX_VALUE ((1u << HB_OT_MAP_MAX_BITS) - 1u)

struct hb_ot_shape_plan_t;

static const hb_tag_t table_tags[2] = {HB_OT_TAG_GSUB, HB_OT_TAG_GPOS};

struct hb_ot_map_t
{
  friend struct hb_ot_map_builder_t;

  public:

  struct feature_map_t {
    hb_tag_t tag; /* should be first for our bsearch to work */
    unsigned int index[2]; /* GSUB/GPOS */
    unsigned int stage[2]; /* GSUB/GPOS */
    unsigned int shift;
    hb_mask_t mask;
    hb_mask_t _1_mask; /* mask for value=1, for quick access */
    unsigned int needs_fallback : 1;
    unsigned int auto_zwnj : 1;
    unsigned int auto_zwj : 1;
    unsigned int random : 1;

    inline int cmp (const hb_tag_t *tag_) const
    { return *tag_ < tag ? -1 : *tag_ > tag ? 1 : 0; }
  };

  struct lookup_map_t {
    unsigned short index;
    unsigned short auto_zwnj : 1;
    unsigned short auto_zwj : 1;
    unsigned short random : 1;
    hb_mask_t mask;

    static int cmp (const void *pa, const void *pb)
    {
      const lookup_map_t *a = (const lookup_map_t *) pa;
      const lookup_map_t *b = (const lookup_map_t *) pb;
      return a->index < b->index ? -1 : a->index > b->index ? 1 : 0;
    }
  };

  typedef void (*pause_func_t) (const struct hb_ot_shape_plan_t *plan, hb_font_t *font, hb_buffer_t *buffer);

  struct stage_map_t {
    unsigned int last_lookup; /* Cumulative */
    pause_func_t pause_func;
  };

  inline void init (void)
  {
    memset (this, 0, sizeof (*this));

    features.init ();
    for (unsigned int table_index = 0; table_index < 2; table_index++)
    {
      lookups[table_index].init ();
      stages[table_index].init ();
    }
  }
  inline void fini (void)
  {
    features.fini ();
    for (unsigned int table_index = 0; table_index < 2; table_index++)
    {
      lookups[table_index].fini ();
      stages[table_index].fini ();
    }
  }

  inline hb_mask_t get_global_mask (void) const { return global_mask; }

  inline hb_mask_t get_mask (hb_tag_t feature_tag, unsigned int *shift = nullptr) const {
    const feature_map_t *map = features.bsearch (feature_tag);
    if (shift) *shift = map ? map->shift : 0;
    return map ? map->mask : 0;
  }

  inline bool needs_fallback (hb_tag_t feature_tag) const {
    const feature_map_t *map = features.bsearch (feature_tag);
    return map ? map->needs_fallback : false;
  }

  inline hb_mask_t get_1_mask (hb_tag_t feature_tag) const {
    const feature_map_t *map = features.bsearch (feature_tag);
    return map ? map->_1_mask : 0;
  }

  inline unsigned int get_feature_index (unsigned int table_index, hb_tag_t feature_tag) const {
    const feature_map_t *map = features.bsearch (feature_tag);
    return map ? map->index[table_index] : HB_OT_LAYOUT_NO_FEATURE_INDEX;
  }

  inline unsigned int get_feature_stage (unsigned int table_index, hb_tag_t feature_tag) const {
    const feature_map_t *map = features.bsearch (feature_tag);
    return map ? map->stage[table_index] : (unsigned int) -1;
  }

  inline void get_stage_lookups (unsigned int table_index, unsigned int stage,
				 const struct lookup_map_t **plookups, unsigned int *lookup_count) const {
    if (unlikely (stage == (unsigned int) -1)) {
      *plookups = nullptr;
      *lookup_count = 0;
      return;
    }
    assert (stage <= stages[table_index].len);
    unsigned int start = stage ? stages[table_index][stage - 1].last_lookup : 0;
    unsigned int end   = stage < stages[table_index].len ? stages[table_index][stage].last_lookup : lookups[table_index].len;
    *plookups = end == start ? nullptr : &lookups[table_index][start];
    *lookup_count = end - start;
  }

  HB_INTERNAL void collect_lookups (unsigned int table_index, hb_set_t *lookups) const;
  template <typename Proxy>
  HB_INTERNAL inline void apply (const Proxy &proxy,
				 const struct hb_ot_shape_plan_t *plan, hb_font_t *font, hb_buffer_t *buffer) const;
  HB_INTERNAL void substitute (const struct hb_ot_shape_plan_t *plan, hb_font_t *font, hb_buffer_t *buffer) const;
  HB_INTERNAL void position (const struct hb_ot_shape_plan_t *plan, hb_font_t *font, hb_buffer_t *buffer) const;

  public:
  hb_tag_t chosen_script[2];
  bool found_script[2];

  private:

  hb_mask_t global_mask;

  hb_vector_t<feature_map_t, 8> features;
  hb_vector_t<lookup_map_t, 32> lookups[2]; /* GSUB/GPOS */
  hb_vector_t<stage_map_t, 4> stages[2]; /* GSUB/GPOS */
};

enum hb_ot_map_feature_flags_t
{
  F_NONE		= 0x0000u,
  F_GLOBAL		= 0x0001u, /* Feature applies to all characters; results in no mask allocated for it. */
  F_HAS_FALLBACK	= 0x0002u, /* Has fallback implementation, so include mask bit even if feature not found. */
  F_MANUAL_ZWNJ		= 0x0004u, /* Don't skip over ZWNJ when matching **context**. */
  F_MANUAL_ZWJ		= 0x0008u, /* Don't skip over ZWJ when matching **input**. */
  F_MANUAL_JOINERS	= F_MANUAL_ZWNJ | F_MANUAL_ZWJ,
  F_GLOBAL_MANUAL_JOINERS= F_GLOBAL | F_MANUAL_JOINERS,
  F_GLOBAL_SEARCH	= 0x0010u, /* If feature not found in LangSys, look for it in global feature list and pick one. */
  F_RANDOM		= 0x0020u  /* Randomly select a glyph from an AlternateSubstFormat1 subtable. */
};
HB_MARK_AS_FLAG_T (hb_ot_map_feature_flags_t);


struct hb_ot_map_feature_t
{
  hb_tag_t tag;
  hb_ot_map_feature_flags_t flags;
};


struct hb_ot_map_builder_t
{
  public:

  HB_INTERNAL hb_ot_map_builder_t (hb_face_t *face_,
				   const hb_segment_properties_t *props_);

  HB_INTERNAL ~hb_ot_map_builder_t (void);

  HB_INTERNAL void add_feature (hb_tag_t tag,
				hb_ot_map_feature_flags_t flags=F_NONE,
				unsigned int value=1);

  inline void add_feature (const hb_ot_map_feature_t &feat)
  { add_feature (feat.tag, feat.flags); }

  inline void enable_feature (hb_tag_t tag)
  { add_feature (tag, F_GLOBAL); }

  inline void disable_feature (hb_tag_t tag)
  { add_feature (tag, F_GLOBAL, 0); }

  inline void add_gsub_pause (hb_ot_map_t::pause_func_t pause_func)
  { add_pause (0, pause_func); }
  inline void add_gpos_pause (hb_ot_map_t::pause_func_t pause_func)
  { add_pause (1, pause_func); }

  HB_INTERNAL void compile (hb_ot_map_t  &m,
			    const int    *coords,
			    unsigned int  num_coords);

  private:

  HB_INTERNAL void add_lookups (hb_ot_map_t  &m,
				unsigned int  table_index,
				unsigned int  feature_index,
				unsigned int  variations_index,
				hb_mask_t     mask,
				bool          auto_zwnj = true,
				bool          auto_zwj = true,
				bool          random = false);

  struct feature_info_t {
    hb_tag_t tag;
    unsigned int seq; /* sequence#, used for stable sorting only */
    unsigned int max_value;
    hb_ot_map_feature_flags_t flags;
    unsigned int default_value; /* for non-global features, what should the unset glyphs take */
    unsigned int stage[2]; /* GSUB/GPOS */

    static int cmp (const void *pa, const void *pb)
    {
      const feature_info_t *a = (const feature_info_t *) pa;
      const feature_info_t *b = (const feature_info_t *) pb;
      return (a->tag != b->tag) ?  (a->tag < b->tag ? -1 : 1) :
	     (a->seq < b->seq ? -1 : a->seq > b->seq ? 1 : 0);
    }
  };

  struct stage_info_t {
    unsigned int index;
    hb_ot_map_t::pause_func_t pause_func;
  };

  HB_INTERNAL void add_pause (unsigned int table_index, hb_ot_map_t::pause_func_t pause_func);

  public:

  hb_face_t *face;
  hb_segment_properties_t props;

  hb_tag_t chosen_script[2];
  bool found_script[2];
  unsigned int script_index[2], language_index[2];

  private:

  unsigned int current_stage[2]; /* GSUB/GPOS */
  hb_vector_t<feature_info_t, 32> feature_infos;
  hb_vector_t<stage_info_t, 8> stages[2]; /* GSUB/GPOS */
};



#endif /* HB_OT_MAP_HH */
