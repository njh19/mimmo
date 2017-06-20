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
#include "ScaleGeometry.hpp"

namespace mimmo{


/*!
 * Default constructor of ScaleGeometry
 */
ScaleGeometry::ScaleGeometry(darray3E scaling){
    m_scaling = scaling;
    m_origin.fill(0.0);
    m_meanP = false;
    m_name = "mimmo.ScaleGeometry";
};

/*!
 * Custom constructor reading xml data
 * \param[in] rootXML reference to your xml tree section
 */
ScaleGeometry::ScaleGeometry(const bitpit::Config::Section & rootXML){

    m_scaling.fill(1.0);
    m_origin.fill(0.0);
    m_meanP = false;
    m_name = "mimmo.ScaleGeometry";

    std::string fallback_name = "ClassNONE";
    std::string input = rootXML.get("ClassName", fallback_name);
    input = bitpit::utils::string::trim(input);
    if(input == "mimmo.ScaleGeometry"){
        absorbSectionXML(rootXML);
    }else{
        warningXML(m_log, m_name);
    };
}

/*!Default destructor of ScaleGeometry
 */
ScaleGeometry::~ScaleGeometry(){};

/*!Copy constructor of ScaleGeometry.
 */
ScaleGeometry::ScaleGeometry(const ScaleGeometry & other):BaseManipulation(other){
    m_scaling = other.m_scaling;
    m_origin = other.m_origin;
    m_meanP = other.m_meanP;;
};

/*! It builds the input/output ports of the object
 */
void
ScaleGeometry::buildPorts(){
    bool built = true;
    built = (built && createPortIn<darray3E, ScaleGeometry>(&m_origin, PortType::M_POINT, mimmo::pin::containerTAG::ARRAY3, mimmo::pin::dataTAG::FLOAT));
    built = (built && createPortIn<darray3E, ScaleGeometry>(&m_scaling, PortType::M_SPAN, mimmo::pin::containerTAG::ARRAY3, mimmo::pin::dataTAG::FLOAT));
    built = (built && createPortIn<dmpvector1D, ScaleGeometry>(this, &mimmo::ScaleGeometry::setFilter, PortType::M_FILTER, mimmo::pin::containerTAG::VECTOR, mimmo::pin::dataTAG::FLOAT));
    built = (built && createPortIn<MimmoObject*, ScaleGeometry>(&m_geometry, PortType::M_GEOM, mimmo::pin::containerTAG::SCALAR, mimmo::pin::dataTAG::MIMMO_, true));
    built = (built && createPortOut<dmpvecarr3E, ScaleGeometry>(this, &mimmo::ScaleGeometry::getDisplacements, PortType::M_GDISPLS, mimmo::pin::containerTAG::VECARR3, mimmo::pin::dataTAG::FLOAT));
    built = (built && createPortOut<MimmoObject*, ScaleGeometry>(this, &BaseManipulation::getGeometry, PortType::M_GEOM, mimmo::pin::containerTAG::SCALAR, mimmo::pin::dataTAG::MIMMO_));
    m_arePortsBuilt = built;
};

/*!It sets the center point.
 * \param[in] origin origin for scaling transform
 */
void
ScaleGeometry::setOrigin(darray3E origin){
    m_origin = origin;
}

/*!It sets if the center point for scaling is the mean point of the geometry.
 * \param[in] meanP origin for scaling transform is the mean point?
 */
void
ScaleGeometry::setMeanPoint(bool meanP){
    m_meanP = meanP;
}

/*!It sets the scaling factors of each axis.
 * \param[in] scaling scaling factor values for x, y and z absolute axis.
 */
void
ScaleGeometry::setScaling(darray3E scaling){
    m_scaling = scaling;
}

/*!It sets the filter field to modulate the displacements of the vertices
 * of the target geometry.
 * \param[in] filter filter field defined on geometry vertices.
 */
void
ScaleGeometry::setFilter(dmpvector1D filter){
    m_filter = filter;
}

/*!
 * Return actual computed displacements field (if any) for the geometry linked.
 * \return  deformation field
 */
dmpvecarr3E
ScaleGeometry::getDisplacements(){
    return m_displ;
};

/*!Execution command. It perform the scaling by computing the displacements
 * of the points of the geometry. It applies a filter field eventually set as input.
 */
void
ScaleGeometry::execute(){

    if (getGeometry() == NULL) return;

    int nV = m_geometry->getNVertex();
    if (m_filter.size() != nV){
        m_filter.clear();
        for (auto vertex : m_geometry->getVertices()){
            m_filter.insert(vertex.getId(), 1.0);
        }
    }

    m_displ.clear();

    long ID;
    darray3E value;
    //computing centroid
    darray3E center = m_origin;
    if (m_meanP){
        center.fill(0.0);
        for (auto vertex : m_geometry->getVertices()){
            ID = vertex.getId();
            center += vertex.getCoords() / double(nV);
        }
    }
    for (auto vertex : m_geometry->getVertices()){
        ID = vertex.getId();

        darray3E coords = vertex.getCoords();
        value = ( m_scaling*(coords - center) + center ) * m_filter[ID] - coords;
        m_displ.insert(ID, value);
    }
};

/*!
 * Directly apply deformation field to target geometry.
 */
void
ScaleGeometry::apply(){

    if (getGeometry() == NULL) return;
    darray3E vertexcoords;
    long int ID;
    for (auto vertex : m_geometry->getVertices()){
        vertexcoords = vertex.getCoords();
        ID = vertex.getId();
        vertexcoords += m_displ[ID];
        getGeometry()->modifyVertex(vertexcoords, ID);
    }

}

/*!
 * It sets infos reading from a XML bitpit::Config::section.
 * \param[in] slotXML bitpit::Config::Section of XML file
 * \param[in] name   name associated to the slot
 */
void
ScaleGeometry::absorbSectionXML(const bitpit::Config::Section & slotXML, std::string name){

    BITPIT_UNUSED(name);

    BaseManipulation::absorbSectionXML(slotXML, name);
    
    if(slotXML.hasOption("MeanPoint")){
        std::string input = slotXML.get("MeanPoint");
        bool value = false;
        if(!input.empty()){
            std::stringstream ss(bitpit::utils::string::trim(input));
            ss >> value;
        }
        setMeanPoint(value);
    };


    if(slotXML.hasOption("Origin")){
        std::string input = slotXML.get("Origin");
        input = bitpit::utils::string::trim(input);
        darray3E temp = {{0.0,0.0,0.0}};
        if(!input.empty()){
            std::stringstream ss(input);
            for(auto &val : temp) ss>>val;
        }
        setOrigin(temp);
    }

    if(slotXML.hasOption("Scaling")){
        std::string input = slotXML.get("Scaling");
        input = bitpit::utils::string::trim(input);
        darray3E temp = {{0.0,0.0,0.0}};
        if(!input.empty()){
            std::stringstream ss(input);
            for(auto &val : temp) ss>>val;
        }
        setScaling(temp);
    }

};

/*!
 * It sets infos from class members in a XML bitpit::Config::section.
 * \param[in] slotXML bitpit::Config::Section of XML file
 * \param[in] name   name associated to the slot
 */
void
ScaleGeometry::flushSectionXML(bitpit::Config::Section & slotXML, std::string name){

    BITPIT_UNUSED(name);

    BaseManipulation::flushSectionXML(slotXML, name);
    {
        std::stringstream ss;
        ss<<std::scientific<<m_origin[0]<<'\t'<<m_origin[1]<<'\t'<<m_origin[2];
        slotXML.set("Origin", ss.str());
    }

    {
        bool value = m_meanP;
        std::string towrite = std::to_string(value);
        slotXML.set("MeanPoint", towrite);
    }

    {
        std::stringstream ss;
        ss<<std::scientific<<m_scaling[0]<<'\t'<<m_scaling[1]<<'\t'<<m_scaling[2];
        slotXML.set("Scaling", ss.str());
    }


};

}
