/*---------------------------------------------------------------------------*\
 *
 *  mimmo
 *
 *  Copyright (C) 2015-2017 OPTIMAD engineering Srl
 *
 *  -------------------------------------------------------------------------
 *  License
 *  This file is part of mimmo.
 *
 *  mimmo is free software: you can redistribute it and/or modify it
 *  under the terms of the GNU Lesser General Public License v3 (LGPL)
 *  as published by the Free Software Foundation.
 *
 *  mimmo is distributed in the hope that it will be useful, but WITHOUT
 *  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 *  FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public
 *  License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public License
 *  along with mimmo. If not, see <http://www.gnu.org/licenses/>.
 *
\*---------------------------------------------------------------------------*/

#include "SwitchFields.hpp"

using namespace std;
using namespace bitpit;
namespace mimmo{

/*!
 * Default constructor.
 */
SwitchScalarField::SwitchScalarField():SwitchField(){
    m_name = "mimmo.SwitchScalarField";
}

/*!
 * Custom constructor reading xml data
 * \param[in] rootXML reference to your xml tree section
 */
SwitchScalarField::SwitchScalarField(const bitpit::Config::Section & rootXML){

    std::string fallback_name = "ClassNONE";
    std::string input_name = rootXML.get("ClassName", fallback_name);
    input_name = bitpit::utils::trim(input_name);

    m_name = "mimmo.SwitchScalarField";

    if(input_name == "mimmo.SwitchScalarField"){
        absorbSectionXML(rootXML);
    }else{
        warningXML(m_log, m_name);
    };
}

/*!
 * Default destructor
 */
SwitchScalarField::~SwitchScalarField(){
    m_fields.clear();
    m_result.clear();
}

/*!
 * Build the ports of the class;
 */
void
SwitchScalarField::buildPorts(){
    bool built = true;
    built = (built && createPortIn<std::vector<dmpvector1D>, SwitchScalarField>(this, &mimmo::SwitchScalarField::setFields, PortType::M_VECFIELDS, mimmo::pin::containerTAG::VECTOR, mimmo::pin::dataTAG::MPVECFLOAT, true, 1));
    built = (built && createPortIn<dmpvector1D, SwitchScalarField>(this, &mimmo::SwitchScalarField::addField, PortType::M_SCALARFIELD, mimmo::pin::containerTAG::MPVECTOR, mimmo::pin::dataTAG::FLOAT, true, 1));
    built = (built && createPortOut<dmpvector1D, SwitchScalarField>(this, &mimmo::SwitchScalarField::getSwitchedField, PortType::M_SCALARFIELD, mimmo::pin::containerTAG::MPVECTOR, mimmo::pin::dataTAG::FLOAT));

    SwitchField::buildPorts();
    m_arePortsBuilt = built;
}

/*!
 * Get switchted field.
 * \return switched field
 */
dmpvector1D
SwitchScalarField::getSwitchedField(){
    return m_result;
}


/*!
 * Set Field associated to the target geometry and that need to switchted.
 * If the field is associated to the cells or to points of the target geometry,
 * please set this info, choosing the correct division map between setCellDivisionMap or 
 * setVertDivisionMap methods.  
 * \param[in]    field vector field of array at 3 double elements
 */
void
SwitchScalarField::setFields(vector<dmpvector1D> fields){
    m_fields.insert(m_fields.end(), fields.begin(), fields.end());
}

/*!
 * Set Field associated to the target geometry and that need to switchted.
 * If the field is associated to the cells or to points of the target geometry,
 * please set this info, choosing the correct division map between setCellDivisionMap or
 * setVertDivisionMap methods.
 * \param[in]    field vector field of array at 3 double elements
 */
void
SwitchScalarField::addField(dmpvector1D field){
    m_fields.push_back(field);
}

/*!
 * Clear content of the class
 */
void
SwitchScalarField::clear(){
    m_fields.clear();
    m_result.clear();
    SwitchField::clear();
}

/*!
 * Plot switchted field alongside its geometries ;
 */
void 
SwitchScalarField::plotOptionalResults(){

    if (m_result.size() == 0 || getGeometry() == NULL) return;

    bitpit::VTKLocation loc = bitpit::VTKLocation::POINT;

    dvector1D field;
    for (auto & v : getGeometry()->getVertices()){
        field.push_back(m_result[v.getId()]);
    }

    bitpit::VTKElementType cellType = getGeometry()->desumeElement();
    liimap mapDataInv;
    dvecarr3E points = getGeometry()->getVertexCoords(&mapDataInv);

    if (cellType == bitpit::VTKElementType::UNDEFINED) return;

    if(cellType != bitpit::VTKElementType::VERTEX){
        ivector2D connectivity = getGeometry()->getCompactConnectivity(mapDataInv);
        bitpit::VTKUnstructuredGrid output(".",m_name+std::to_string(getClassCounter()),cellType);
        output.setGeomData( bitpit::VTKUnstructuredField::POINTS, points);
        output.setGeomData( bitpit::VTKUnstructuredField::CONNECTIVITY, connectivity);
        output.setDimensions(connectivity.size(), points.size());
        output.addData("field", bitpit::VTKFieldType::SCALAR, loc, field);
        output.setCodex(bitpit::VTKFormat::APPENDED);
        output.write();
    }else{
        int size = points.size();
        ivector1D connectivity(size);
        for(int i=0; i<size; ++i)    connectivity[i]=i;
        bitpit::VTKUnstructuredGrid output(".",m_name+std::to_string(getClassCounter()),    cellType);
        output.setGeomData( bitpit::VTKUnstructuredField::POINTS, points);
        output.setGeomData( bitpit::VTKUnstructuredField::CONNECTIVITY, connectivity);
        output.setDimensions(connectivity.size(), points.size());
        output.addData("field", bitpit::VTKFieldType::SCALAR, loc, field);
        output.setCodex(bitpit::VTKFormat::APPENDED);
        output.write();
    }

}


/*!
 * Switch your original field along the switch original geometries provided
 * \return true if switch without errors
 */
bool
SwitchScalarField::mswitch(){

    if (getGeometry() == NULL) return false;

    m_result.clear();

    //Extract by link to geometry
    for (auto field : m_fields){

        if (field.getGeometry() == NULL ) return false;

        if (field.getGeometry() == getGeometry()){
            m_result = field;
            m_result.setGeometry(getGeometry());
            m_result.setName(field.getName());
            return true;
        }
    }

    //Extract by geometric mapping
    if (m_mapping){
        double tol = 1.0e-08;
        for (auto field : m_fields){
            livector1D result = mimmo::bvTreeUtils::selectByPatch(field.getGeometry()->getBvTree(), getGeometry()->getBvTree(), tol);
            if (result.size() != 0) m_result.setName(field.getName());
            for (auto idC : result){
                for (auto id : getGeometry()->getCellConnectivity(idC)){
                    if (!m_result.exists(id))
                        m_result.insert(id, field[id]);
                }
            }
        }
    }

    if (m_result.size() == 0) return false;

    m_result.setGeometry(getGeometry());

    return true;
}

}