//===- Z3Mgr.cpp -- Z3 manager for symbolic execution ------------------//
//
//                     SVF: Static Value-Flow Analysis
//
// Copyright (C) <2013-2022>  <Yulei Sui>
//

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Affero General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Affero General Public License for more details.

// You should have received a copy of the GNU Affero General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
//===----------------------------------------------------------------------===//
/*
 * Z3 manager for symbolic execution
 *
 * Created on: Feb 19, 2024
 */

#include "Z3SSEMgr.h"
#include "SVF-LLVM/LLVMUtil.h"
#include "SVFIR/SVFIR.h"
#include <iomanip>
#include <set>

using namespace SVF;
using namespace SVFUtil;
using namespace llvm;
using namespace z3;

Z3SSEMgr::Z3SSEMgr(SVFIR* ir)
: Z3Mgr(ir->getPAGNodeNum() * 10)
, svfir(ir) {
	initMap();
}

/*
 * (1) Create Z3 expressions for top-level variables and objects, and (2) create loc2val map
 */
void Z3SSEMgr::initMap() {
	for (SVFIR::iterator nIter = svfir->begin(); nIter != svfir->end(); ++nIter) {
		if (const ValVar* val = SVFUtil::dyn_cast<ValVar>(nIter->second))
			createExprForValVar(val);
		else if (const ObjVar* obj = SVFUtil::dyn_cast<ObjVar>(nIter->second))
			createExprForObjVar(obj);
		else
			assert(false && "SVFVar type not supported");
	}
	DBOP(printExprValues());
}

/*
 * We initialize all ValVar to be an int expression.
 * ValVar of PointerTy/isFunctionTy is assigned with an ID to represent the address of an object
 * ValVar of IntegerTy is assigned with an integer value
 * ValVar of FloatingPointTy is assigned with an integer value casted from a float
 * ValVar of StructTy/ArrayTy is assigned with their elements of the above types in the form of integer values
 */
z3::expr Z3SSEMgr::createExprForValVar(const ValVar* valVar) {
	std::string str;
	raw_string_ostream rawstr(str);
	rawstr << "ValVar" << valVar->getId();
	expr e(ctx);
	if (const SVFType* type = valVar->getType()) {
		e = ctx.int_const(rawstr.str().c_str());
	}
	else {
		e = ctx.int_const(rawstr.str().c_str());
		assert(SVFUtil::isa<DummyValVar>(valVar) && "not a DummValVar if it has no type?");
	}

	updateZ3Expr(valVar->getId(), e);
	return e;
}

/*
 * Object must be either a constraint data or a location value (address-taken variable)
 * Constant data includes ConstantInt, ConstantFP, ConstantPointerNull and ConstantAggregate
 * Locations includes pointers to globals, heaps, stacks
 */
z3::expr Z3SSEMgr::createExprForObjVar(const ObjVar* objVar) {
	std::string str;
	raw_string_ostream rawstr(str);
	rawstr << "ObjVar" << objVar->getId();

	expr e = ctx.int_const(rawstr.str().c_str());
	if (objVar->hasValue()) {
		const MemObj* obj = objVar->getMemObj();
		/// constant data
		if (obj->isConstDataOrAggData() || obj->isConstantArray() || obj->isConstantStruct()) {
			if (const SVFConstantInt* consInt = SVFUtil::dyn_cast<SVFConstantInt>(obj->getValue())) {
				e = ctx.int_val((s32_t)consInt->getSExtValue());
			}
			else if (const SVFConstantFP* consFP = SVFUtil::dyn_cast<SVFConstantFP>(obj->getValue()))
				e = ctx.int_val(static_cast<u32_t>(consFP->getFPValue()));
			else if (SVFUtil::isa<SVFConstantNullPtr>(obj->getValue()))
				e = ctx.int_val(0);
			else if (SVFUtil::isa<SVFGlobalValue>(obj->getValue()))
				e = ctx.int_val(getVirtualMemAddress(objVar->getId()));
			else if (obj->isConstantArray() || obj->isConstantStruct())
				assert(false && "implement this part");
			else {
				std::cerr << obj->getValue()->toString() << "\n";
				assert(false && "what other types of values we have?");
			}
		}
		/// locations (address-taken variables)
		else {
			e = ctx.int_val(getVirtualMemAddress(objVar->getId()));
		}
	}
	else {
		assert(SVFUtil::isa<DummyObjVar>(objVar)
		       && "it should either be a blackhole or constant dummy if this obj has no value?");
		e = ctx.int_val(getVirtualMemAddress(objVar->getId()));
	}

	updateZ3Expr(objVar->getId(), e);
	return e;
}

/// Return the address expr of a ObjVar
z3::expr Z3SSEMgr::getMemObjAddress(u32_t idx) const {
	NodeID objIdx = getInternalID(idx);
	assert(SVFUtil::isa<ObjVar>(svfir->getGNode(objIdx)) && "Fail to get the MemObj!");
	return getZ3Expr(objIdx);
}

z3::expr Z3SSEMgr::getGepObjAddress(z3::expr pointer, u32_t offset) {
	NodeID baseObj = getInternalID(z3Expr2NumValue(pointer));
	assert(SVFUtil::isa<ObjVar>(svfir->getGNode(baseObj)) && "Fail to get the base object address!");
	NodeID gepObj = svfir->getGepObjVar(baseObj, offset);
	/// TODO: check whether this node has been created before or not to save creation time
	if (baseObj == gepObj)
		return getZ3Expr(baseObj);
	else
		return createExprForObjVar(SVFUtil::cast<GepObjVar>(svfir->getGNode(gepObj)));
}

s32_t Z3SSEMgr::getGepOffset(const GepStmt* gep) {
	if (gep->getOffsetVarAndGepTypePairVec().empty())
		return gep->getConstantStructFldIdx();

	s32_t totalOffset = 0;
	for (int i = gep->getOffsetVarAndGepTypePairVec().size() - 1; i >= 0; i--) {
		const SVFValue* value = gep->getOffsetVarAndGepTypePairVec()[i].first->getValue();
		const SVFType* type = gep->getOffsetVarAndGepTypePairVec()[i].second;
		const SVFConstantInt* op = SVFUtil::dyn_cast<SVFConstantInt>(value);
		s32_t offset = 0;
		/// constant as the offset
		if (op)
			offset = op->getSExtValue();
		/// variable as the offset
		else
			offset = z3Expr2NumValue(getZ3Expr(svfir->getValueNode(value)));

		if (type == nullptr) {
			totalOffset += offset;
			continue;
		}

		/// Calculate the offset
		if (const SVFPointerType* pty = SVFUtil::dyn_cast<SVFPointerType>(type))
			totalOffset += offset * gep->getAccessPath().getElementNum(gep->getAccessPath().gepSrcPointeeType());
		else
			totalOffset += SymbolTableInfo::SymbolInfo()->getFlattenedElemIdx(type, offset);
	}
	return totalOffset;
}

void Z3SSEMgr::printExprValues() {
	std::cout.flags(std::ios::left);
	std::cout << "-----------SVFVar and Value-----------\n";
	std::map<std::string, std::string> printValMap;
	std::map<NodeID, std::string> objKeyMap;
	std::map<NodeID, std::string> valKeyMap;
	for (SVFIR::iterator nIter = svfir->begin(); nIter != svfir->end(); ++nIter) {
		expr e = getEvalExpr(getZ3Expr(nIter->first));
		if (e.is_numeral()) {
			NodeID varID = nIter->second->getId();
			s32_t value = e.get_numeral_int64();
			std::stringstream exprName;
			std::stringstream valstr;
			if (const ValVar* valVar = SVFUtil::dyn_cast<ValVar>(nIter->second)) {
				exprName << "ValVar" << varID;
				if (isVirtualMemAddress(value))
					valstr << "\t Value: " << std::hex << "0x" << value << "\n";
				else
					valstr << "\t Value: " << std::dec << value << "\n";
				printValMap[exprName.str()] = valstr.str();
				valKeyMap[varID] = exprName.str();
			}
			else {
				exprName << "ObjVar" << varID << std::hex << " (0x" << getVirtualMemAddress(varID) << ") ";
				if (isVirtualMemAddress(value)) {
					expr storedValue = getEvalExpr(loadValue(e));
					if (storedValue.is_numeral()) {
						s32_t contentValue = z3Expr2NumValue(storedValue);
						if (isVirtualMemAddress(contentValue))
							valstr << "\t Value: " << std::hex << "0x" << contentValue << "\n";
						else
							valstr << "\t Value: " << std::dec << contentValue << "\n";
					}
					else
						valstr << "\t Value: NULL"
						       << "\n";
				}
				else
					valstr << "\t Value: NULL"
					       << "\n";
				printValMap[exprName.str()] = valstr.str();
				objKeyMap[varID] = exprName.str();
			}
		}
	}
	for (auto it = objKeyMap.begin(); it != objKeyMap.end(); ++it) {
		std::string printKey = it->second;
		std::cout << std::setw(25) << printKey << printValMap[printKey];
	}
	for (auto it = valKeyMap.begin(); it != valKeyMap.end(); ++it) {
		std::string printKey = it->second;
		std::cout << std::setw(25) << printKey << printValMap[printKey];
	}
	std::cout << "-----------------------------------------\n";
}