# OpenCPN Spatial Index Task Prompt Template

Copy and paste this prompt at the beginning of each task chat session, replacing `{TASK_FILE}` with the specific task file name.

---

## Context

I'm working on implementing a high-performance spatial indexing system for OpenCPN as described in the requirements. This is a complex maritime navigation project that requires careful analysis and integration with existing systems.

**Project Overview**: Please read `/Users/serosset/git/OpenCPN/requirements/README.md` for the complete project context, goals, and maritime navigation safety requirements.

**Current Task**: Please read `/Users/serosset/git/OpenCPN/requirements/{TASK_FILE}` for the specific task requirements and acceptance criteria.

**Code Access**: The complete OpenCPN codebase is accessible through filesystem MCP under `/Users/serosset/git/OpenCPN`. Please examine the existing code thoroughly before making any recommendations.

## Quality Expectations

This is a mission-critical navigation safety system with very high quality requirements:

- **Safety First**: Any errors could impact maritime navigation safety
- **Integration Focus**: Solutions must integrate seamlessly with existing OpenCPN systems
- **Performance Critical**: The system must achieve 10-100x performance improvements
- **Code Quality**: All code must meet marine-grade software standards

## Approach Requirements

**DO NOT jump into implementation.** Instead:

1. **Analyze First**: Thoroughly examine the existing OpenCPN code related to the task
2. **Ask Questions**: When in doubt about existing patterns, data structures, or integration points, ask clarifying questions
3. **Understand Context**: Review how current systems work before proposing changes
4. **Plan Integration**: Ensure any proposed solution fits well with existing architecture
5. **Consider Edge Cases**: Maritime navigation has many edge cases (date line crossing, datum conversions, etc.)

## Expected Process

1. Read and understand the project README and specific task requirements
2. Examine relevant existing code files and data structures
3. Ask clarifying questions about unclear aspects
4. Propose an analysis or design approach for review
5. Only proceed with implementation after the approach is validated

Please start by confirming you've read the project README and task requirements, then begin your analysis of the existing codebase relevant to this task.

---

## Task-Specific Examples

### For Task 1.1 (Analysis):
Replace `{TASK_FILE}` with: `task-1.1-analyze-spatial-data-systems.md`

### For Task 2.1 (Implementation):
Replace `{TASK_FILE}` with: `task-2.1-implement-production-spatial-index.md`

### For Task 4.1 (Integration):
Replace `{TASK_FILE}` with: `task-4.1-replace-gshhs-crossesland.md`
