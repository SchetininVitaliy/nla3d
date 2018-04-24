// This file is a part of nla3d project. For information about authors and
// licensing go to project's repository on github:
// https://github.com/dmitryikh/nla3d 

#include "VtkProcessor.h"
#include "elements/element.h"

namespace nla3d {
using namespace math;

VtkProcessor::VtkProcessor(FEStorage *st, std::string _fileName) : PostProcessor(st) {
	name = "VtkProcessor";
	file_name = _fileName;
}

VtkProcessor::~VtkProcessor() {
}

void VtkProcessor::pre(){
  useDisplacementsDofs = false;
  nodalDofTypes.clear();
  elementDofTypes.clear();

  // find which nodal Dofs used in FEStorage and add to list nodalDofTypes for printing out into VTK
  // file
  auto u_dofs = storage->getUniqueNodeDofTypes();
  for (auto& type : u_dofs) {
    // take care of displacement dofs: if one of them exist in storage we need to write 3x1 vector
    // into VTK (for wrapping)
    if (type == Dof::UX || type == Dof::UY || type == Dof::UZ) {
      useDisplacementsDofs = true;
      continue;
    }
    nodalDofTypes.insert(type);
  }

  // the same for Element dofs
  u_dofs = storage->getUniqueElementDofTypes();
  for (auto& type : u_dofs) {
    elementDofTypes.insert(type);
  }

  // if was requested reveal query codes to be written into VTK
  if(_writeAllResults) {
    revealAllResults();
  }

  // write zero VTK file (before solution)
	std::string cur_fn = file_name + "0"  + ".vtk";
	std::ofstream file(cur_fn.c_str(), std::ios::trunc);
	write_header(file);
	write_geometry(file);
	write_point_data(file);
	write_cell_data(file);
	file.close();
}

void VtkProcessor::process (uint16 curLoadstep) {
	std::string cur_fn = file_name + toStr(curLoadstep) + ".vtk";
	std::ofstream file(cur_fn.c_str(), std::ios::trunc);
	write_header(file);
	write_geometry(file);
	write_point_data(file);
	write_cell_data(file);
	file.close();
}

void VtkProcessor::post (uint16 curLoadstep) {
}


void VtkProcessor::writeAllResults(bool write) {
  _writeAllResults = write;
}


bool VtkProcessor::registerResults(std::initializer_list<scalarQuery> queries) {
  // check that each query is valid
  for(auto v : queries) {
    assert(v > scalarQuery::UNDEF && v < scalarQuery::LAST);
  }

  cellScalarQueries.insert(queries);
  return true;
}


bool VtkProcessor::registerResults(std::initializer_list<vectorQuery> queries) {
  // check that each query is valid
  for(auto v : queries) {
    assert(v > vectorQuery::UNDEF && v < vectorQuery::LAST);
  }

  cellVectorQueries.insert(queries);
  return true;
}


bool VtkProcessor::registerResults(std::initializer_list<tensorQuery> queries) {
  // check that each query is valid
  for(auto v : queries) {
    assert(v > tensorQuery::UNDEF && v < tensorQuery::LAST);
  }

  cellTensorQueries.insert(queries);
  return true;
}


void VtkProcessor::write_header (std::ofstream &file) {
	file << "# vtk DataFile Version 2.0" << std::endl;
	file << "This file is generated by nla3D program" << std::endl << "ASCII" << std::endl;
	file << "DATASET UNSTRUCTURED_GRID" << std::endl;
}

void VtkProcessor::write_geometry(std::ofstream &file, bool def) {
	Vec<3> xi;
	file << "POINTS " << storage->nNodes() << " float" << std::endl;
	for (uint32 i=1; i <= storage->nNodes(); i++)
	{
		storage->getNodePosition(i, xi.ptr(), def);
		file << xi << std::endl;
	}
	/*
	CELLS en en*9
	4 i j k l

	CELL_TYPES en
	11
	11
  */

  // calculate overall size of element data
  uint32 data_size = 0;
	for (uint32 i=1; i <= storage->nElements(); i++)
              data_size += storage->getElement(i).getNNodes() + 1;

	file << "CELLS " << storage->nElements() << " " <<  data_size << std::endl;
	for (uint32 i=1; i <= storage->nElements(); i++)
	{
    uint16 nodesNum = storage->getElement(i).getNNodes();
		file << nodesNum;
		for (uint16 j=0; j < nodesNum; j++) 
			file << " " << storage->getElement(i).getNodeNumber(j)-1;
		file << std::endl;
	}
	file << "CELL_TYPES " << storage->nElements() << std::endl;
	for (uint32 i=1; i <= storage->nElements(); i++) {
    ElementShape eltype = storage->getElement(i).getShape();
    if (eltype == ElementShape::QUAD) {
      file << "9" << std::endl; //VTK_QUAD, see VTK file formats
    } else if (eltype == ElementShape::HEXAHEDRON) {
      file << "12" << std::endl; //VTK_HEXAHEDRON
    } else if (eltype == ElementShape::TETRA) {
      file << "10" << std::endl; //VTK_TETRA
    } else if (eltype == ElementShape::TRIANGLE) {
      file << "5" << std::endl; //VTK_TRIANGLE
    } else if (eltype == ElementShape::LINE) {
      file << "3" << std::endl; //VTK_LINE
    } else if (eltype == ElementShape::VERTEX) {
      file << "2" << std::endl; //VTK_POLY_VERTEX
    } else if (eltype == ElementShape::WEDGE) {
      file << "13" << std::endl; //VTK_WEDGE
    }  else {
      LOG_N_TIMES(10, ERROR) << "Don't now what type of elements here it is (el_num = " << i << ")";
    }
  }
}

void VtkProcessor::write_point_data(std::ofstream &file) {
  size_t nn = storage->nNodes();
  file << "POINT_DATA " << nn << std::endl;
  std::vector<Vec<3> > dataVector;
  std::vector<double> dataScalar;

  if (useDisplacementsDofs) {
    dataVector.assign(nn, Vec<3> ());
    for (uint32 j = 1; j <= nn; j++) {
      if (storage->isNodeDofUsed(j, Dof::UX)) {
        dataVector[j-1][0] = storage->getNodeDofSolution(j, Dof::UX);
      }
      if (storage->isNodeDofUsed(j, Dof::UY)) {
        dataVector[j-1][1] = storage->getNodeDofSolution(j, Dof::UY);
      }
      if (storage->isNodeDofUsed(j, Dof::UZ)) {
        dataVector[j-1][2] = storage->getNodeDofSolution(j, Dof::UZ);
      }
    }
    writeVector(file, "disp", dataVector);

    for (uint32 j = 1; j <= nn; j++) {
      dataVector[j-1][0] = storage->getReaction(j, Dof::UX);
      dataVector[j-1][1] = storage->getReaction(j, Dof::UY);
      dataVector[j-1][2] = storage->getReaction(j, Dof::UZ);
    }
    writeVector(file, "reaction", dataVector);

    dataVector.clear();
  }

  for (auto& v : nodalDofTypes) {
    dataScalar.assign(nn, 0.0);
    for (uint32 j = 1; j <= nn; j++) {
      if (storage->isNodeDofUsed(j, v)) {
        dataScalar[j-1] = storage->getNodeDofSolution(j, v);
      }
    }
    writeScalar(file, Dof::dofType2label(v), dataScalar);
    dataScalar.clear();
  }
}

//Write cell data averaging from all integration points.
//Use global coordinate system. all futher transformations 
//should be done on paraview side. 
void VtkProcessor::write_cell_data(std::ofstream &file) {
  size_t en = storage->nElements();
  std::vector<MatSym<3> > dataTensor;
  std::vector<Vec<3> > dataVector;
  std::vector<double> dataScalar;

  // if queries are empty - no cell data to be stored into VTK
  if (cellScalarQueries.size() == 0 && cellVectorQueries.size() == 0 &&
      cellTensorQueries.size() == 0 && elementDofTypes.size() == 0) {
    return;
  }

	file << "CELL_DATA " << en << std::endl;
  
  // write element DoFs
  for (auto& v : elementDofTypes) {
    dataScalar.assign(en, 0.0);
    for (uint32 j = 1; j <= en; j++) {
      if (storage->isElementDofUsed(j, v)) {
        dataScalar[j-1] = storage->getElementDofSolution(j, v);
      }
    }
    writeScalar(file, Dof::dofType2label(v), dataScalar);
    dataScalar.clear();
  }

  // write Element scalar results (only if they are requested in cellScalarQueries)
  for(auto query : cellScalarQueries) {
    dataScalar.assign(en, 0.0);
    for (uint32 i = 1; i <= storage->nElements(); i++) {
      // dataScalar[i-1] = 0.0;
      storage->getElement(i).getScalar(&(dataScalar[i-1]), query);
    }
    writeScalar(file, query2label(query), dataScalar); 
  }

  // write Element vector results (only if they are requested in cellScalarQueries)
  for(auto query : cellVectorQueries) {
    dataVector.assign(en, Vec<3>());
    for (uint32 i = 1; i <= storage->nElements(); i++) {
      // dataVector[i-1].zero();
      storage->getElement(i).getVector(&(dataVector[i-1]), query);
    }
    writeVector(file, query2label(query), dataVector); 
  }

  // write Element tensor results (only if they are requested in cellScalarQueries)
  for(auto query : cellTensorQueries) {
    dataTensor.assign(en, MatSym<3>());
    for (uint32 i = 1; i <= storage->nElements(); i++) {
      // dataTensor[i-1].zero();
      storage->getElement(i).getTensor(&(dataTensor[i-1]), query);
    }
    writeTensor(file, query2label(query), dataTensor); 
  }
}


void VtkProcessor::writeScalar(std::ofstream &file, const char* name, std::vector<double>& data) {
  file << "SCALARS " << name <<  " float 1" << std::endl;
  file << "LOOKUP_TABLE default"<< std::endl;
  for (size_t i = 0; i < data.size(); i++) {
    file << data[i] << std::endl;
  }
  file << std::endl;
}


void VtkProcessor::writeTensor(std::ofstream &file, const char* name, std::vector<MatSym<3> >& data) {
    for (size_t j = 0; j < 6; j++) {
      file << "SCALARS " << name << "_" << solidmech::labelsTensorComponent[j] << " float 1" << std::endl;
      file << "LOOKUP_TABLE default"<< std::endl;
      for (size_t i = 0; i < data.size(); i++) {
        file << data[i].data[j] << std::endl;
      }
      file << std::endl;
    }
}


void VtkProcessor::writeVector(std::ofstream &file, const char* name, std::vector<Vec<3> >& data) {
  file << "VECTORS " << name << " float" << std::endl;
  for (size_t i = 0; i < data.size(); i++) {
    file << data[i] << std::endl;
  }
  file << std::endl;

}


void VtkProcessor::revealAllResults() {
  std::set<ElementType> typesRevealed;
  bool ret;

  for (uint32 i = 1; i <= storage->nElements(); i++) {
    Element& el = storage->getElement(i);
    ElementType etype = el.getType();
    if(typesRevealed.find(etype) != typesRevealed.end()) continue;

    // remember that we already revealed the elements of type `etype`
    typesRevealed.insert(etype);
    // This code doesn't work on -O2 optimized code under clang..
    // Here is a kludge - we just print out the etype number..
    LOG(INFO) << static_cast<int>(etype);
    double sdummy = 0.0;
    scalarQuery squery = scalarQuery::UNDEF;
    // Loop over scalarQuery items
    while(true) {
      squery = static_cast<scalarQuery>(static_cast<int>(squery) + 1);
      if(squery == scalarQuery::LAST) break;
      sdummy = 0.0;
      // try to ask the element about particular query code. If result is false, than this element
      // know nothing about the query code, skip the code.
      ret = el.getScalar(&sdummy, squery);
      if(ret == true) {
        cellScalarQueries.insert(squery);
      }
    }

    Vec<3> vdummy;
    vectorQuery vquery = vectorQuery::UNDEF;
    // Loop over vectorQuery items
    while(true) {
      vquery = static_cast<vectorQuery>(static_cast<int>(vquery) + 1);
      if(vquery == vectorQuery::LAST) break;
      vdummy.zero();
      // try to ask the element about particular query code. If result is false, than this element
      // know nothing about the query code, skip the code.
      ret = el.getVector(&vdummy, vquery);
      if(ret == true) {
        cellVectorQueries.insert(vquery);
      }
    }

    MatSym<3> mdummy;
    tensorQuery tquery = tensorQuery::UNDEF;
    // Loop over tensorQuery items
    while(true) {
      tquery = static_cast<tensorQuery>(static_cast<int>(tquery) + 1);
      if(tquery == tensorQuery::LAST) break;
      mdummy.zero();
      // try to ask the element about particular query code. If result is false, than this element
      // know nothing about the query code, skip the code.
      ret = el.getTensor(&mdummy, tquery);
      if(ret == true) {
        cellTensorQueries.insert(tquery);
      }
    }
  }

}


} // namespace nla3d
