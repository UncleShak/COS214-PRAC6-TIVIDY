/**
 * @file WorkflowVisitor.h
 * @brief Visitor participant: one report or check that walks the work tree.
 *
 * TIVIDY needs different views over a running workflow (progress, overdue work, audit),
 * and every organisation wants different ones. Each view is a ConcreteVisitor, so a new
 * report never changes WorkItem, Stage or WorkflowInstance.
 *
 * Double dispatch: a client calls root->accept(&visitor). Each element calls the visit
 * operation for its own type:
 *  - WorkItem::accept(v)  calls v->visitWorkItem(this);
 *  - Stage::accept(v)     calls v->visitStage(this), then accept(v) on each child;
 *  - TaskDecorator::accept(v) forwards to the component it wraps (already in the code),
 *    so decorated work is still visited as the real WorkItem.
 *
 * GoF roles: Visitor = WorkflowVisitor; ConcreteVisitor = ProgressReportVisitor,
 * OverdueWorkVisitor, AuditReportVisitor; Element = WorkflowComponent (accept);
 * ConcreteElement = WorkItem, Stage; ObjectStructure = WorkflowInstance (owns the root).
 *
 * @author Ona Mojela (Envy)
 */
#ifndef TIVIDY_WORKFLOW_VISITOR_H
#define TIVIDY_WORKFLOW_VISITOR_H

class WorkItem;
class Stage;

/** @brief Declares one visit operation per kind of element in the work tree. */
class WorkflowVisitor {
public:
    virtual ~WorkflowVisitor() = default;

    /** @brief Called by WorkItem::accept for a single unit of work (Composite leaf). */
    virtual void visitWorkItem(WorkItem* item) = 0;

    /**
     * @brief Called by Stage::accept for a group of work (Composite), before the stage
     *        passes the visitor on to its children.
     */
    virtual void visitStage(Stage* stage) = 0;
};

#endif
