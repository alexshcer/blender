/*
 * Copyright 2013, Blender Foundation.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include <map>
#include <sstream>
#include <string>

#include <Alembic/Abc/IObject.h>
#include <Alembic/Abc/OObject.h>

#include "abc_mesh.h"
#include "abc_group.h"
#include "abc_object.h"

extern "C" {
#include "BLI_math.h"

#include "DNA_group_types.h"
#include "DNA_object_types.h"

#include "BKE_anim.h"
#include "BKE_global.h"
#include "BKE_group.h"
#include "BKE_library.h"
}

namespace PTC {

using namespace Abc;
using namespace AbcGeom;

AbcGroupWriter::AbcGroupWriter(const std::string &name, Group *group) :
    GroupWriter(group, name)
{
}

void AbcGroupWriter::open_archive(WriterArchive *archive)
{
	BLI_assert(dynamic_cast<AbcWriterArchive*>(archive));
	AbcWriter::abc_archive(static_cast<AbcWriterArchive*>(archive));
	
	if (abc_archive()->archive) {
		m_abc_object = abc_archive()->add_id_object<OObject>((ID *)m_group);
	}
}

void AbcGroupWriter::create_refs()
{
	GroupObject *gob = (GroupObject *)m_group->gobject.first;
	int i = 0;
	for (; gob; gob = gob->next, ++i) {
		OObject abc_object = abc_archive()->get_id_object((ID *)gob->ob);
		if (abc_object) {
			std::stringstream ss;
			ss << i;
			m_abc_object.addChildInstance(abc_object, std::string("group_object")+ss.str());
		}
	}
}

void AbcGroupWriter::write_sample()
{
	if (!abc_archive()->archive)
		return;
}


AbcGroupReader::AbcGroupReader(const std::string &name, Group *group) :
    GroupReader(group, name)
{
}

void AbcGroupReader::open_archive(ReaderArchive *archive)
{
	BLI_assert(dynamic_cast<AbcReaderArchive*>(archive));
	AbcReader::abc_archive(static_cast<AbcReaderArchive*>(archive));
	
	if (abc_archive()->archive) {
		m_abc_object = abc_archive()->get_id_object((ID *)m_group);
	}
}

PTCReadSampleResult AbcGroupReader::read_sample(float frame)
{
	if (!m_abc_object)
		return PTC_READ_SAMPLE_INVALID;
	
	return PTC_READ_SAMPLE_EXACT;
}

/* ========================================================================= */

AbcDupligroupWriter::AbcDupligroupWriter(const std::string &name, EvaluationContext *eval_ctx, Scene *scene, Group *group) :
    GroupWriter(group, name),
    m_eval_ctx(eval_ctx),
    m_scene(scene)
{
}

AbcDupligroupWriter::~AbcDupligroupWriter()
{
	for (IDWriterMap::iterator it = m_id_writers.begin(); it != m_id_writers.end(); ++it) {
		if (it->second)
			delete it->second;
	}
}

void AbcDupligroupWriter::open_archive(WriterArchive *archive)
{
	BLI_assert(dynamic_cast<AbcWriterArchive*>(archive));
	AbcWriter::abc_archive(static_cast<AbcWriterArchive*>(archive));
	
	if (abc_archive()->archive) {
		m_abc_group = abc_archive()->add_id_object<OObject>((ID *)m_group);
	}
}

void AbcDupligroupWriter::write_sample_object(Object *ob)
{
	Writer *ob_writer = find_id_writer((ID *)ob);
	if (!ob_writer) {
		ob_writer = new AbcObjectWriter(ob->id.name, ob);
		ob_writer->set_archive(m_archive);
		m_id_writers.insert(IDWriterPair((ID *)ob, ob_writer));
	}
	
	ob_writer->write_sample();
}

void AbcDupligroupWriter::write_sample_dupli(DupliObject *dob, int index)
{
	OObject abc_object = abc_archive()->get_id_object((ID *)dob->ob);
	if (!abc_object)
		return;
	
	std::stringstream ss;
	ss << "DupliObject" << index;
	std::string name = ss.str();
	
	OObject abc_dupli = m_abc_group.getChild(name);
	OCompoundProperty props;
	OM44fProperty prop_matrix;
	if (!abc_dupli) {
		abc_dupli = OObject(m_abc_group, name, 0);
		m_object_writers.push_back(abc_dupli.getPtr());
		props = abc_dupli.getProperties();
		
		abc_dupli.addChildInstance(abc_object, "object");
		
		prop_matrix = OM44fProperty(props, "matrix", 0);
		m_property_writers.push_back(prop_matrix.getPtr());
	}
	else {
		props = abc_dupli.getProperties();
		
		prop_matrix = OM44fProperty(props.getProperty("matrix").getPtr()->asScalarPtr(), kWrapExisting);
	}
	
	prop_matrix.set(M44f(dob->mat));
}

void AbcDupligroupWriter::write_sample()
{
	if (!m_abc_group)
		return;
	
	ListBase *duplilist = group_duplilist_ex(m_eval_ctx, m_scene, m_group, true);
	DupliObject *dob;
	int i;
	
	/* LIB_DOIT is used to mark handled objects, clear first */
	for (dob = (DupliObject *)duplilist->first; dob; dob = dob->next) {
		if (dob->ob)
			dob->ob->id.flag &= ~LIB_DOIT;
	}
	
	/* write actual object data: duplicator itself + all instanced objects */
	for (dob = (DupliObject *)duplilist->first; dob; dob = dob->next) {
		if (dob->ob->id.flag & LIB_DOIT)
			continue;
		dob->ob->id.flag |= LIB_DOIT;
		
		write_sample_object(dob->ob);
	}
	
	/* write dupli instances */
	for (dob = (DupliObject *)duplilist->first, i = 0; dob; dob = dob->next, ++i) {
		write_sample_dupli(dob, i);
	}
	
	free_object_duplilist(duplilist);
}

Writer *AbcDupligroupWriter::find_id_writer(ID *id) const
{
	IDWriterMap::const_iterator it = m_id_writers.find(id);
	if (it == m_id_writers.end())
		return NULL;
	else
		return it->second;
}

/* ------------------------------------------------------------------------- */

typedef float Matrix[4][4];

typedef float (*MatrixPtr)[4];

static Matrix I = {{1.0f, 0.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 0.0f, 1.0f}};

struct DupliGroupContext {
	typedef std::map<ObjectReaderPtr, DupliObjectData*> DupliMap;
	typedef std::pair<ObjectReaderPtr, DupliObjectData*> DupliPair;
	
	struct Transform {
		Transform() {}
		Transform(float (*value)[4]) { copy_m4_m4(matrix, value); }
		Transform(const Transform &tfm) { memcpy(matrix, tfm.matrix, sizeof(Matrix)); }
		
		Matrix matrix;
	};
	typedef std::vector<Transform> TransformStack;
	
	typedef std::map<std::string, Object*> ObjectMap;
	typedef std::pair<std::string, Object*> ObjectPair;
	
	/* constructor */
	DupliGroupContext(DupliCache *dupli_cache) :
	    dupli_cache(dupli_cache)
	{
		tfm_stack.push_back(Transform(I));
	}
	
	
	DupliObjectData *find_dupli_data(ObjectReaderPtr ptr) const
	{
		DupliMap::const_iterator it = dupli_map.find(ptr);
		if (it == dupli_map.end())
			return NULL;
		else
			return it->second;
	}
	
	void insert_dupli_data(ObjectReaderPtr ptr, DupliObjectData *data)
	{
		dupli_map.insert(DupliPair(ptr, data));
	}
	
	
	MatrixPtr get_transform() { return tfm_stack.back().matrix; }
//	void push_transform(float mat[4][4])
	
	
	void build_object_map(Main *bmain, Group *group)
	{
		BKE_main_id_tag_idcode(bmain, ID_OB, false);
		BKE_main_id_tag_idcode(bmain, ID_GR, false);
		object_map.clear();
		
		build_object_map_add_group(group);
	}
	
	Object *find_object(const std::string &name) const
	{
		ObjectMap::const_iterator it = object_map.find(name);
		if (it == object_map.end())
			return NULL;
		else
			return it->second;
	}
	
	DupliMap dupli_map;
	DupliCache *dupli_cache;
	
	TransformStack tfm_stack;
	
	ObjectMap object_map;
	
protected:
	void build_object_map_add_group(Group *group)
	{
		if (group->id.flag & LIB_DOIT)
			return;
		group->id.flag |= LIB_DOIT;
		
		for (GroupObject *gob = (GroupObject *)group->gobject.first; gob; gob = gob->next) {
			Object *ob = gob->ob;
			if (ob->id.flag & LIB_DOIT)
				continue;
			ob->id.flag |= LIB_DOIT;
			object_map.insert(ObjectPair(ob->id.name, ob));
			
			if ((ob->transflag & OB_DUPLIGROUP) && ob->dup_group) {
				build_object_map_add_group(ob->dup_group);
			}
		}
	}
};

static void read_dupligroup_object(DupliGroupContext &ctx, IObject object, const ISampleSelector &ss)
{
	if (GS(object.getName().c_str()) == ID_OB) {
		/* instances are handled later, we create true object data here */
		if (object.isInstanceDescendant())
			return;
		
		Object *b_ob = ctx.find_object(object.getName());
		if (!b_ob)
			return;
		
		/* TODO load DM, from subobjects for IPolyMesh etc. */
		DerivedMesh *dm = NULL;
		DupliObjectData *data = BKE_dupli_cache_add_mesh(ctx.dupli_cache, b_ob, dm);
		ctx.insert_dupli_data(object.getPtr(), data);
	}
}

static void read_dupligroup_group(DupliGroupContext &ctx, IObject abc_group, const ISampleSelector &ss)
{
	if (GS(abc_group.getName().c_str()) == ID_GR) {
		size_t num_child = abc_group.getNumChildren();
		
		for (size_t i = 0; i < num_child; ++i) {
			IObject abc_dupli = abc_group.getChild(i);
			ICompoundProperty props = abc_dupli.getProperties();
			
			IM44fProperty prop_matrix(props, "matrix", 0);
			M44f abc_matrix = prop_matrix.getValue(ss);
			float matrix[4][4];
			memcpy(matrix, abc_matrix.getValue(), sizeof(float)*4*4);
			
			IObject abc_dupli_object = abc_dupli.getChild("object");
			if (abc_dupli_object.isInstanceRoot()) {
				DupliObjectData *dupli_data = ctx.find_dupli_data(abc_dupli_object.getPtr());
				if (dupli_data) {
					BKE_dupli_cache_add_instance(ctx.dupli_cache, matrix, dupli_data);
				}
			}
		}
	}
}

PTCReadSampleResult abc_read_dupligroup(ReaderArchive *_archive, float frame, Group *dupgroup, DupliCache *dupcache)
{
	AbcReaderArchive *archive = (AbcReaderArchive *)_archive;
	DupliGroupContext ctx(dupcache);
	
	/* XXX this mapping allows fast lookup of existing objects in Blender data
	 * to associate with duplis. Later i may be possible to create instances of
	 * non-DNA data, but for the time being this is a requirement due to other code parts (drawing, rendering)
	 */
	ctx.build_object_map(G.main, dupgroup);
	
	ISampleSelector ss = archive->get_frame_sample_selector(frame);
	
	IObject abc_top = archive->archive.getTop();
	IObject abc_group = archive->get_id_object((ID *)dupgroup);
	if (!abc_group)
		return PTC_READ_SAMPLE_INVALID;
	
	/* first create shared object data */
	for (size_t i = 0; i < abc_top.getNumChildren(); ++i) {
		read_dupligroup_object(ctx, abc_top.getChild(i), ss);
	}
	
	/* now generate dupli instances for the dupgroup */
	read_dupligroup_group(ctx, abc_group, ss);
	
	return PTC_READ_SAMPLE_EXACT;
}

} /* namespace PTC */
