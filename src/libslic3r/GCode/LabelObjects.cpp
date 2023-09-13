#include "LabelObjects.hpp"


namespace Slic3r::GCode {



void LabelObjects::init(const Print& print)
{
    m_label_objects_style = print.config().gcode_label_objects;
    m_flavor = print.config().gcode_flavor;

    if (m_label_objects_style == LabelObjectsStyle::Disabled)
        return;

    std::map<const ModelObject*, std::vector<const PrintInstance*>> model_object_to_print_instances;

    // Iterate over all PrintObjects and their PrintInstances, collect PrintInstances which
    // belong to the same ModelObject.
    for (const PrintObject* po : print.objects())
        for (const PrintInstance& pi : po->instances())
            model_object_to_print_instances[pi.model_instance->get_object()].emplace_back(&pi);
    
    // Now go through the map, assign a unique_id to each of the PrintInstances and get the indices of the
    // respective ModelObject and ModelInstance so we can use them in the tags. This will maintain
    // indices even in case that some instances are rotated (those end up in different PrintObjects)
    // or when some are out of bed (these ModelInstances have no corresponding PrintInstances).
    int unique_id = 0;
    for (const auto& [model_object, print_instances] : model_object_to_print_instances) {
        const ModelObjectPtrs& model_objects = model_object->get_model()->objects;
        int object_id = int(std::find(model_objects.begin(), model_objects.end(), model_object) - model_objects.begin());
        for (const PrintInstance* const pi : print_instances) {
            bool object_has_more_instances = print_instances.size() > 1u;
            int instance_id = int(std::find(model_object->instances.begin(), model_object->instances.end(), pi->model_instance) - model_object->instances.begin());

            // Now compose the name of the object and define whether indexing is 0 or 1-based.
            std::string name = model_object->name;
            if (m_label_objects_style == LabelObjectsStyle::Octoprint) {
                // use zero-based indexing for objects and instances, as we always have done
                name += " id:" + std::to_string(object_id) + " copy " + std::to_string(instance_id); 
            }
            else if (m_label_objects_style == LabelObjectsStyle::Firmware) {
                // use one-based indexing for objects and instances so indices match what we see in PrusaSlicer.
                ++object_id;
                ++instance_id;

                if (object_has_more_instances)
                    name += " (Instance " + std::to_string(instance_id) + ")";
            }

            m_label_data.emplace(pi, LabelData{name, unique_id});
            ++unique_id;
        }
    }
}



std::string LabelObjects::all_objects_header() const
{
    if (m_label_objects_style == LabelObjectsStyle::Disabled)
        return std::string();

    std::string out;

    // Let's sort the values according to unique_id so they are in the same order in which they were added.
    std::vector<std::pair<const PrintInstance*, LabelData>> label_data_sorted;
    for (const auto& pi_and_label : m_label_data)
        label_data_sorted.emplace_back(pi_and_label);
    std::sort(label_data_sorted.begin(), label_data_sorted.end(), [](const auto& ld1, const auto& ld2) { return ld1.second.unique_id < ld2.second.unique_id; });

    out += "\n";
    for (const auto& [print_instance, label] : label_data_sorted) {
        out += start_object(*print_instance, IncludeName::Yes);
        out += stop_object(*print_instance);
    }
    out += "\n";
    return out;
}



std::string LabelObjects::start_object(const PrintInstance& print_instance, IncludeName include_name) const
{
    if (m_label_objects_style == LabelObjectsStyle::Disabled)
        return std::string();

    const LabelData& label = m_label_data.at(&print_instance);

    std::string out;
    if (m_label_objects_style == LabelObjectsStyle::Octoprint)
        out += std::string("; printing object ") + label.name + "\n";
    else if (m_label_objects_style == LabelObjectsStyle::Firmware) {
        if (m_flavor == GCodeFlavor::gcfMarlinFirmware || m_flavor == GCodeFlavor::gcfMarlinLegacy || m_flavor == GCodeFlavor::gcfRepRapFirmware) {
            out += std::string("M486 S") + std::to_string(label.unique_id) + "\n";
            if (include_name == IncludeName::Yes) {
                out += std::string("M486 A");
                out += (m_flavor == GCodeFlavor::gcfRepRapFirmware ? (std::string("\"") + label.name + "\"") : label.name) + "\n";
            }
        } else {
            // Not supported by / implemented for the other firmware flavors.
        }
    }
    return out;
}



std::string LabelObjects::stop_object(const PrintInstance& print_instance) const
{
    if (m_label_objects_style == LabelObjectsStyle::Disabled)
        return std::string();

    const LabelData& label = m_label_data.at(&print_instance);

    std::string out;
    if (m_label_objects_style == LabelObjectsStyle::Octoprint)
        out += std::string("; stop printing object ") + label.name + "\n";
    else if (m_label_objects_style == LabelObjectsStyle::Firmware)
        if (m_flavor == GCodeFlavor::gcfMarlinFirmware || m_flavor == GCodeFlavor::gcfMarlinLegacy || m_flavor == GCodeFlavor::gcfRepRapFirmware)
            out += std::string("M486 S-1\n");
        else {
            // Not supported by / implemented for the other firmware flavors.
        }
    return out;
}



} // namespace Slic3r::GCode
