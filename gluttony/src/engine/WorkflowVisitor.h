/**
 * @file WorkflowVisitor.h
 * @brief STAND-IN for the Visitor interface (owned by another member). Only what the
 *        decorators need to forward accept() calls.
 *
 * @author Tanatswa Masiiwa (Gluttony)
 * @note Implemented with assistance from Claude (Anthropic AI), from the team's Practical 6 UML.
 */
#ifndef TIVIDY_WORKFLOW_VISITOR_H
#define TIVIDY_WORKFLOW_VISITOR_H

class WorkItem;

/** @brief Visitor over workflow components (stand-in: WorkItem only). */
class WorkflowVisitor {
public:
    virtual ~WorkflowVisitor() {}
    /** @brief Called by WorkItem::accept(). */
    virtual void visitWorkItem(WorkItem* item) = 0;
};

#endif
